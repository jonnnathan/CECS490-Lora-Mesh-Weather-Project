/*
 * Enhanced Mesh Topology Visualization for LoRa Dashboard
 *
 * Features:
 * - Draw connection lines based on actual neighbor relationships
 * - Color-code lines by RSSI strength (green=strong, yellow=fair, red=weak)
 * - Show hop count badges on node markers
 * - Animated updates when topology changes
 * - Tooltips showing connection quality
 *
 * INTEGRATION NOTES:
 * This code enhances the existing updateMap() function in web_dashboard.cpp
 * It can visualize mesh topology using the meshSenderId field to show direct connections
 */

// ============================================================================
// ENHANCED MAP UPDATE FUNCTION
// ============================================================================

function updateMapWithMeshTopology(nodes, gateway) {
    // Remove old connection lines
    for (const lineId of Object.keys(connectionLines)) {
        map.removeLayer(connectionLines[lineId]);
        delete connectionLines[lineId];
    }

    let gatewayCoords = null;
    let mapInitialized = markers[gateway.nodeId] !== undefined;

    // First pass: Update all node markers with hop count badges
    for (const [id, node] of Object.entries(nodes)) {
        const nodeId = parseInt(id);

        // Calculate hop count from mesh TTL
        // Initial TTL is 3, each hop decrements it
        const hopCount = node.meshTtl ? (3 - node.meshTtl) : 0;

        if (node.lat === 0 && node.lng === 0) continue;

        const coords = [node.lat, node.lng];
        const isGateway = nodeId === gateway.nodeId;

        if (isGateway) {
            gatewayCoords = coords;
        }

        // Create enhanced icon with hop count
        const iconColor = node.online ? (isGateway ? '#00d4ff' : '#44ff44') : '#ff4444';
        const hopBadge = hopCount > 0 ? `<div style="position:absolute;top:-5px;right:-5px;background:#ff8800;border-radius:50%;width:18px;height:18px;font-size:10px;display:flex;align-items:center;justify-content:center;font-weight:bold;border:2px solid #1a1a2e;">${hopCount}</div>` : '';

        const iconHtml = `
            <div style="position:relative;">
                <div style="background:${iconColor};border:3px solid #1a1a2e;border-radius:50%;width:32px;height:32px;display:flex;align-items:center;justify-content:center;font-weight:bold;color:#000;box-shadow:0 0 10px ${iconColor};">
                    ${nodeId}
                </div>
                ${hopBadge}
            </div>
        `;

        const icon = L.divIcon({
            html: iconHtml,
            className: 'custom-marker',
            iconSize: [32, 32],
            iconAnchor: [16, 16]
        });

        // Enhanced popup with mesh info
        const popup = '<div style="color:#000;min-width:180px;">' +
            '<strong>Node ' + nodeId + '</strong> ' +
            '<span style="color:' + (node.online ? '#0a0' : '#a00') + '">●</span><br>' +
            (hopCount > 0 ? '<small style="color:#ff8800;">Hop Count: ' + hopCount + '</small><br>' : '') +
            '<small style="color:#888;">Via Node: ' + node.meshSenderId + '</small><br>' +
            '<small style="color:#888;">Source: ' + node.meshSourceId + '</small><br>' +
            '<small style="color:#888;">TTL: ' + node.meshTtl + '</small><br>' +
            '<hr style="margin:5px 0;border:none;border-top:1px solid #ddd;">' +
            '<small style="color:#888;">Sensor Data:</small><br>' +
            'Temp: ' + node.temp + '°F<br>' +
            'Humidity: ' + node.humidity + '%<br>' +
            'Neighbors: ' + (node.neighborCount || 0) + ' nodes<br>' +
            (isGateway ? '' : 'RSSI: ' + node.rssi + ' dBm<br>') +
            '</div>';

        // Update or create marker
        if (markers[id]) {
            markers[id].setLatLng(coords);
            markers[id].setIcon(icon);
            markers[id].setPopupContent(popup);
        } else {
            markers[id] = L.marker(coords, {icon: icon}).addTo(map).bindPopup(popup);
        }

        // Center map on first valid coordinate
        if (!mapInitialized) {
            map.setView(coords, 17);
            mapInitialized = true;
        }
    }

    // Second pass: Draw mesh topology connections
    // Use meshSenderId to show who forwarded the packet (direct connection)
    const drawnConnections = new Set(); // Prevent duplicate lines

    for (const [id, node] of Object.entries(nodes)) {
        if (node.lat === 0 && node.lng === 0) continue;
        if (!node.online) continue;

        const toId = parseInt(id);
        const fromId = node.meshSenderId;

        // Skip if this is direct from source (no forwarding)
        if (toId === node.meshSourceId) continue;

        // Find the sender node's coordinates
        const senderNode = nodes[fromId];
        if (!senderNode || senderNode.lat === 0 || senderNode.lng === 0) continue;

        const fromCoords = [senderNode.lat, senderNode.lng];
        const toCoords = [node.lat, node.lng];

        // Create unique line identifier (bidirectional)
        const lineKey = [Math.min(fromId, toId), Math.max(fromId, toId)].join('-');
        if (drawnConnections.has(lineKey)) continue;
        drawnConnections.add(lineKey);

        const lineId = 'mesh-' + lineKey;

        // Determine line style based on RSSI
        const rssi = node.rssi || -100;
        let lineColor, lineWeight, dashArray, lineOpacity;

        if (rssi > -60) {
            // Excellent signal
            lineColor = '#44ff44';
            lineWeight = 4;
            dashArray = null;
            lineOpacity = 0.8;
        } else if (rssi > -70) {
            // Good signal
            lineColor = '#00d4ff';
            lineWeight = 3;
            dashArray = null;
            lineOpacity = 0.7;
        } else if (rssi > -80) {
            // Fair signal
            lineColor = '#ffaa00';
            lineWeight = 2;
            dashArray = '10, 5';
            lineOpacity = 0.6;
        } else if (rssi > -90) {
            // Weak signal
            lineColor = '#ff6600';
            lineWeight = 2;
            dashArray = '5, 5';
            lineOpacity = 0.5;
        } else {
            // Very weak
            lineColor = '#ff4444';
            lineWeight = 1;
            dashArray = '3, 3';
            lineOpacity = 0.4;
        }

        const lineStyle = {
            color: lineColor,
            weight: lineWeight,
            opacity: lineOpacity,
            dashArray: dashArray
        };

        // Create or update polyline
        if (connectionLines[lineId]) {
            connectionLines[lineId].setLatLngs([fromCoords, toCoords]);
            connectionLines[lineId].setStyle(lineStyle);
        } else {
            connectionLines[lineId] = L.polyline([fromCoords, toCoords], lineStyle).addTo(map);

            // Add tooltip with connection quality
            const signalQuality = rssi > -60 ? 'Excellent' :
                                rssi > -70 ? 'Good' :
                                rssi > -80 ? 'Fair' :
                                rssi > -90 ? 'Weak' : 'Very Weak';

            connectionLines[lineId].bindTooltip(
                `Node ${fromId} ↔ Node ${toId}<br>` +
                `RSSI: ${rssi} dBm<br>` +
                `Signal: ${signalQuality}`,
                {
                    permanent: false,
                    direction: 'center',
                    className: 'connection-tooltip'
                }
            );
        }
    }

    // Remove old lines that no longer exist
    for (const lineId of Object.keys(connectionLines)) {
        const lineKey = lineId.replace('mesh-', '');
        if (!drawnConnections.has(lineKey)) {
            map.removeLayer(connectionLines[lineId]);
            delete connectionLines[lineId];
        }
    }
}

// ============================================================================
// RSSI COLOR LEGEND (Add to HTML)
// ============================================================================

/*
Add this HTML to the dashboard (in the map tab div):

<div style="position:absolute;bottom:20px;right:20px;background:rgba(26,26,46,0.95);padding:15px;border-radius:8px;border:1px solid #00d4ff33;z-index:1000;">
    <div style="font-weight:bold;margin-bottom:10px;color:#00d4ff;">Signal Strength Legend</div>
    <div style="display:flex;align-items:center;margin:5px 0;">
        <div style="width:40px;height:3px;background:#44ff44;margin-right:10px;"></div>
        <span style="font-size:12px;">Excellent (&gt; -60 dBm)</span>
    </div>
    <div style="display:flex;align-items:center;margin:5px 0;">
        <div style="width:40px;height:3px;background:#00d4ff;margin-right:10px;"></div>
        <span style="font-size:12px;">Good (-60 to -70 dBm)</span>
    </div>
    <div style="display:flex;align-items:center;margin:5px 0;">
        <div style="width:40px;height:3px;background:#ffaa00;margin-right:10px;"></div>
        <span style="font-size:12px;">Fair (-70 to -80 dBm)</span>
    </div>
    <div style="display:flex;align-items:center;margin:5px 0;">
        <div style="width:40px;height:3px;background:#ff6600;margin-right:10px;"></div>
        <span style="font-size:12px;">Weak (-80 to -90 dBm)</span>
    </div>
    <div style="display:flex;align-items:center;margin:5px 0;">
        <div style="width:40px;height:3px;background:#ff4444;margin-right:10px;"></div>
        <span style="font-size:12px;">Very Weak (&lt; -90 dBm)</span>
    </div>
    <hr style="margin:10px 0;border:none;border-top:1px solid #00d4ff33;">
    <div style="display:flex;align-items:center;margin:5px 0;">
        <div style="width:18px;height:18px;background:#ff8800;border-radius:50%;display:flex;align-items:center;justify-content:center;font-size:10px;font-weight:bold;margin-right:10px;border:2px solid #1a1a2e;">2</div>
        <span style="font-size:12px;">Hop Count Badge</span>
    </div>
</div>
*/

// ============================================================================
// ADVANCED: NEIGHBOR TABLE VISUALIZATION (REQUIRES BACKEND CHANGES)
// ============================================================================

/*
To show actual neighbor relationships (not just forwarding paths),
you need to add neighbor table data to the JSON API.

Add this to generateJSON() in web_dashboard.cpp after neighborCount:

// Neighbor list (if available)
json += ",\"neighbors\":[";
bool firstNeighbor = true;
for (uint8_t n = 1; n <= MAX_NEIGHBORS; n++) {
    Neighbor* neighbor = neighborTable.get(n);
    if (neighbor && neighbor->isActive) {
        if (!firstNeighbor) json += ",";
        firstNeighbor = false;
        json += "{";
        json += "\"nodeId\":" + String(neighbor->nodeId) + ",";
        json += "\"rssi\":" + String(neighbor->rssi) + ",";
        json += "\"rssiMin\":" + String(neighbor->rssiMin) + ",";
        json += "\"rssiMax\":" + String(neighbor->rssiMax) + ",";
        json += "\"packets\":" + String(neighbor->packetsReceived);
        json += "}";
    }
}
json += "]";

Then use this enhanced function:
*/

function updateMapWithFullNeighborTopology(nodes, gateway) {
    // Clear old lines
    for (const lineId of Object.keys(connectionLines)) {
        map.removeLayer(connectionLines[lineId]);
        delete connectionLines[lineId];
    }

    const drawnConnections = new Set();

    // Draw connections based on neighbor table data
    for (const [id, node] of Object.entries(nodes)) {
        if (!node.online || !node.neighbors) continue;

        const fromId = parseInt(id);
        const fromNode = nodes[fromId];
        if (!fromNode || fromNode.lat === 0 || fromNode.lng === 0) continue;

        const fromCoords = [fromNode.lat, fromNode.lng];

        // Draw line to each neighbor
        for (const neighbor of node.neighbors) {
            const toId = neighbor.nodeId;
            const toNode = nodes[toId];
            if (!toNode || toNode.lat === 0 || toNode.lng === 0) continue;

            // Create unique line identifier
            const lineKey = [Math.min(fromId, toId), Math.max(fromId, toId)].join('-');
            if (drawnConnections.has(lineKey)) continue;
            drawnConnections.add(lineKey);

            const toCoords = [toNode.lat, toNode.lng];
            const lineId = 'neighbor-' + lineKey;

            // Use neighbor RSSI for line styling
            const rssi = neighbor.rssi;
            const lineStyle = getRSSILineStyle(rssi);

            connectionLines[lineId] = L.polyline([fromCoords, toCoords], lineStyle).addTo(map);
            connectionLines[lineId].bindTooltip(
                `Node ${fromId} ↔ Node ${toId}<br>` +
                `RSSI: ${rssi} dBm<br>` +
                `Range: ${neighbor.rssiMin} to ${neighbor.rssiMax} dBm<br>` +
                `Packets: ${neighbor.packets}`,
                { permanent: false, direction: 'center' }
            );
        }
    }
}

function getRSSILineStyle(rssi) {
    let lineColor, lineWeight, dashArray, lineOpacity;

    if (rssi > -60) {
        lineColor = '#44ff44'; lineWeight = 4; dashArray = null; lineOpacity = 0.8;
    } else if (rssi > -70) {
        lineColor = '#00d4ff'; lineWeight = 3; dashArray = null; lineOpacity = 0.7;
    } else if (rssi > -80) {
        lineColor = '#ffaa00'; lineWeight = 2; dashArray = '10, 5'; lineOpacity = 0.6;
    } else if (rssi > -90) {
        lineColor = '#ff6600'; lineWeight = 2; dashArray = '5, 5'; lineOpacity = 0.5;
    } else {
        lineColor = '#ff4444'; lineWeight = 1; dashArray = '3, 3'; lineOpacity = 0.4;
    }

    return {
        color: lineColor,
        weight: lineWeight,
        opacity: lineOpacity,
        dashArray: dashArray
    };
}

// ============================================================================
// CSS STYLES (Add to <style> section)
// ============================================================================

/*
.connection-tooltip {
    background: rgba(26, 26, 46, 0.95) !important;
    border: 1px solid #00d4ff !important;
    border-radius: 4px !important;
    color: #e0e0e0 !important;
    padding: 8px !important;
    font-size: 12px !important;
}

.custom-marker {
    transition: transform 0.3s ease;
}

.custom-marker:hover {
    transform: scale(1.2);
    z-index: 1000 !important;
}
*/

// ============================================================================
// USAGE INSTRUCTIONS
// ============================================================================

/*
STEP 1: Replace the existing updateMap() call in updateData() with:
    updateMapWithMeshTopology(data.nodes, data.gateway);

STEP 2: Add the CSS styles to the <style> section

STEP 3: Add the RSSI legend HTML to the map tab

STEP 4: (Optional) For full neighbor topology:
    - Add neighbor data to JSON API (see backend changes above)
    - Use updateMapWithFullNeighborTopology() instead

FEATURES:
✅ Shows actual mesh connections based on meshSenderId
✅ Color-coded by RSSI strength (5 levels)
✅ Hop count badges on relayed nodes
✅ Animated tooltips with connection details
✅ Bidirectional connection detection (no duplicate lines)
✅ Dynamic updates as topology changes
✅ Signal strength legend
✅ Dashed lines for weak connections
*/
