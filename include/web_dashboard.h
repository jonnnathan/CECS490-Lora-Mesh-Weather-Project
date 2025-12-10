/*
 * web_dashboard.h
 * WiFi Web Dashboard with GPS Map and Connection Lines
 */

#ifndef WEB_DASHBOARD_H
#define WEB_DASHBOARD_H

#include <Arduino.h>

// Initialize WiFi and web server (only runs on gateway node)
bool initWebDashboard();

// Call this in loop() to handle web requests
void handleWebDashboard();

// Check if dashboard is running
bool isWebDashboardRunning();

// Get the IP address of the gateway
String getGatewayIP();

// Lightweight dashboard for Access Point mode (offline)
bool initWebDashboardLite();
void handleWebDashboardLite();
bool isWebDashboardLiteRunning();

#endif // WEB_DASHBOARD_H