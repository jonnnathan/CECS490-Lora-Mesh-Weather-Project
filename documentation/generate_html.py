import markdown
import os

# Configuration
INPUT_FILE = 'TECHNICAL_DATASHEET_AND_REVIEW.md'
OUTPUT_FILE = 'TECHNICAL_DATASHEET_AND_REVIEW.html'

# CSS Style (Github-like)
CSS_STYLE = """
<style>
    body {
        font-family: -apple-system,BlinkMacSystemFont,"Segoe UI",Helvetica,Arial,sans-serif;
        font-size: 16px;
        line-height: 1.5;
        word-wrap: break-word;
        color: #24292e;
        margin: 0;
        padding: 2rem;
        max-width: 1200px; /* Wider for diagrams */
        margin: 0 auto;
    }
    h1, h2, h3, h4, h5, h6 {
        margin-top: 24px;
        margin-bottom: 16px;
        font-weight: 600;
        line-height: 1.25;
    }
    h1 { font-size: 2em; padding-bottom: 0.3em; border-bottom: 1px solid #eaecef; }
    h2 { font-size: 1.5em; padding-bottom: 0.3em; border-bottom: 1px solid #eaecef; }
    h3 { font-size: 1.25em; }
    code {
        padding: 0.2em 0.4em;
        margin: 0;
        font-size: 85%;
        background-color: rgba(27,31,35,0.05);
        border-radius: 3px;
        font-family: "SFMono-Regular",Consolas,"Liberation Mono",Menlo,Courier,monospace;
    }
    pre {
        padding: 16px;
        overflow: auto;
        font-size: 85%;
        line-height: 1.45;
        background-color: #f6f8fa;
        border-radius: 3px;
    }
    pre code {
        background-color: transparent;
        padding: 0;
    }
    blockquote {
        padding: 0 1em;
        color: #6a737d;
        border-left: 0.25em solid #dfe2e5;
        margin: 0;
    }
    table {
        display: block;
        width: 100%;
        overflow: auto;
        border-spacing: 0;
        border-collapse: collapse;
    }
    table tr {
        background-color: #fff;
        border-top: 1px solid #c6cbd1;
    }
    table tr:nth-child(2n) {
        background-color: #f6f8fa;
    }
    table th, table td {
        padding: 6px 13px;
        border: 1px solid #dfe2e5;
    }
    hr {
        height: 0.25em;
        padding: 0;
        margin: 24px 0;
        background-color: #e1e4e8;
        border: 0;
    }
    a {
        color: #0366d6;
        text-decoration: none;
    }
    a:hover {
        text-decoration: underline;
    }
    .print-button {
        position: fixed;
        top: 20px;
        right: 20px;
        padding: 10px 20px;
        background-color: #0366d6;
        color: white;
        border: none;
        border-radius: 5px;
        cursor: pointer;
        font-weight: bold;
    }
    /* Mermaid Centering */
    .mermaid {
        display: flex;
        justify-content: center;
        margin: 20px 0;
    }
    @media print {
        .print-button { display: none; }
        body { padding: 0; max-width: 100%; }
        a { text-decoration: none; color: black; }
    }
</style>
"""

def convert():
    if not os.path.exists(INPUT_FILE):
        print(f"Error: {INPUT_FILE} not found.")
        return

    with open(INPUT_FILE, 'r', encoding='utf-8') as f:
        text = f.read()

    # Create a custom extension for mermaid code blocks
    # We essentially want to replace ```mermaid ... ``` with <div class="mermaid"> ... </div>
    lines = text.split('\n')
    new_lines = []
    in_mermaid = False
    
    for line in lines:
        if line.strip() == '```mermaid':
            in_mermaid = True
            new_lines.append('<div class="mermaid">')
        elif line.strip() == '```' and in_mermaid:
            in_mermaid = False
            new_lines.append('</div>')
        else:
            new_lines.append(line)
            
    processed_text = '\n'.join(new_lines)

    # Convert to HTML
    html_content = markdown.markdown(processed_text, extensions=['tables', 'fenced_code'])

    # Wrap in HTML structure with Mermaid JS
    full_html = f"""
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <title>Technical Datasheet</title>
    {CSS_STYLE}
    <!-- Mermaid JS -->
    <script src="https://cdn.jsdelivr.net/npm/mermaid/dist/mermaid.min.js"></script>
    <script>
        mermaid.initialize({{ startOnLoad: true }});
    </script>
</head>
<body>
    <button class="print-button" onclick="window.print()">Print to PDF</button>
    {html_content}
</body>
</html>
"""

    with open(OUTPUT_FILE, 'w', encoding='utf-8') as f:
        f.write(full_html)

    print(f"Successfully created {OUTPUT_FILE}")

if __name__ == "__main__":
    convert()
