import * as vscode from 'vscode';
import * as child_process from 'child_process';
import * as path from 'path';

export class RefactorViewProvider implements vscode.WebviewViewProvider {
    public static readonly viewType = 'cppRefactorView';
    private _view?: vscode.WebviewView;
    private _context: vscode.ExtensionContext;

    constructor(context: vscode.ExtensionContext) {
        this._context = context;
    }

    public resolveWebviewView(
        webviewView: vscode.WebviewView,
        context: vscode.WebviewViewResolveContext,
        _token: vscode.CancellationToken,
    ) {
        this._view = webviewView;

        webviewView.webview.options = {
            enableScripts: true,
            localResourceRoots: [this._context.extensionUri]
        };

        webviewView.webview.html = this._getHtml();

        webviewView.webview.onDidReceiveMessage(async (message) => {
            switch (message.command) {
                case 'refactor':
                    await this.runRefactor(message.filePath, message.line, message.column, message.goal, message.showDebug);
                    break;
                case 'getCurrentPosition':
                    this.sendCurrentPosition();
                    break;
            }
        });

        // Отправляем позицию при открытии
        this.sendCurrentPosition();
    }

    private sendCurrentPosition() {
        const editor = vscode.window.activeTextEditor;
        if (editor && editor.document.languageId === 'cpp') {
            const position = editor.selection.active;
            const line = position.line + 1;
            const column = position.character + 1;
            const filePath = editor.document.uri.fsPath;

            this._view?.webview.postMessage({
                command: 'setPosition',
                filePath: filePath,
                line: line,
                column: column
            });
        }
    }

    private async runRefactor(filePath: string, line: number, column: number, goal: string, showDebug: boolean) {
        const scriptPath = path.join(this._context.extensionPath, 'cpp-tool', 'method_analyzer.py');

        this._view?.webview.postMessage({
            command: 'status',
            status: 'processing',
            message: 'Анализ метода...'
        });

        try {
            const result = await new Promise<string>((resolve, reject) => {
                child_process.execFile('python3',
                                       [scriptPath, filePath, String(line), String(column), goal],
                                       { maxBuffer: 1024 * 1024 * 10 },
                                       (error, stdout, stderr) => {
                                           if (error) {
                                               reject(error);
                                               return;
                                           }
                                           resolve(stdout);
                                       }
                );
            });

            let suggestion = '';
            let methodName = '';
            let className = '';
            let usedTypes: string[] = [];
            let sentContext = '';

            try {
                const data = JSON.parse(result);
                suggestion = data.suggestion || data.content || result;
                methodName = data.method || '';
                className = data.class || '';
                usedTypes = data.used_types || [];
                sentContext = data.sent_context || '';
            } catch (e) {
                suggestion = result;
            }

            this._view?.webview.postMessage({
                command: 'result',
                suggestion: suggestion,
                methodName: methodName,
                className: className,
                usedTypes: usedTypes,
                sentContext: sentContext,
                showDebug: showDebug
            });

        } catch (error: any) {
            this._view?.webview.postMessage({
                command: 'error',
                message: error.message
            });
        }
    }

    private _getHtml(): string {
        return `<!DOCTYPE html>
        <html>
        <head>
        <meta charset="UTF-8">
        <style>
        body {
            font-family: var(--vscode-font-family);
            padding: 12px;
            background-color: var(--vscode-editor-background);
            color: var(--vscode-editor-foreground);
        }
        .section {
            margin-bottom: 16px;
            border: 1px solid var(--vscode-panel-border);
            border-radius: 6px;
            padding: 10px;
        }
        .section-title {
            font-weight: bold;
            margin-bottom: 8px;
            font-size: 12px;
            color: var(--vscode-textLink-foreground);
        }
        textarea {
            width: 100%;
            padding: 6px;
            background-color: var(--vscode-input-background);
            color: var(--vscode-input-foreground);
            border: 1px solid var(--vscode-input-border);
            border-radius: 4px;
            font-family: monospace;
            box-sizing: border-box;
            font-size: 11px;
        }
        textarea {
            min-height: 60px;
        }
        .info {
            background-color: var(--vscode-editor-inactiveSelectionBackground);
            padding: 6px;
            border-radius: 4px;
            font-family: monospace;
            font-size: 11px;
            margin-bottom: 8px;
            word-break: break-all;
        }
        button {
            background-color: var(--vscode-button-background);
            color: var(--vscode-button-foreground);
            border: none;
            padding: 6px 12px;
            border-radius: 4px;
            cursor: pointer;
            font-size: 12px;
            width: 100%;
        }
        button:hover {
            background-color: var(--vscode-button-hoverBackground);
        }
        .result {
            background-color: var(--vscode-textCodeBlock-background);
            padding: 8px;
            border-radius: 4px;
            white-space: pre-wrap;
            font-family: monospace;
            font-size: 11px;
            max-height: 300px;
            overflow: auto;
        }
        .status {
            padding: 6px;
            border-radius: 4px;
            margin-top: 8px;
            font-size: 11px;
        }
        .status.processing {
            background-color: var(--vscode-statusBarItem-warningBackground);
        }
        .status.error {
            background-color: var(--vscode-errorForeground);
        }
        .checkbox {
            margin-right: 6px;
        }
        .type-tag {
            background-color: var(--vscode-terminal-ansiBlue);
            color: white;
            padding: 2px 6px;
            border-radius: 10px;
            font-size: 10px;
            display: inline-block;
            margin: 2px;
        }
        .used-types {
            margin-top: 6px;
        }
        .collapsible {
            cursor: pointer;
            user-select: none;
        }
        .collapsible-content {
            display: none;
            margin-top: 8px;
        }
        .collapsible-content.open {
            display: block;
        }
        .context-area {
            background-color: var(--vscode-terminal-background);
            padding: 6px;
            font-family: monospace;
            font-size: 10px;
            white-space: pre-wrap;
            max-height: 200px;
            overflow: auto;
        }
        </style>
        </head>
        <body>
        <div class="section">
        <div class="section-title">📍 Позиция курсора</div>
        <div id="fileInfo" class="info">Откройте C++ файл</div>
        <div id="positionInfo" class="info">-</div>
        </div>

        <div class="section">
        <div class="section-title">🎯 Цель рефакторинга</div>
        <textarea id="goal" placeholder="Например: добавить проверки на nullptr...">Добавить проверки на nullptr и обработку ошибок</textarea>
        </div>

        <div class="section">
        <label>
        <input type="checkbox" id="showDebug" class="checkbox"> Показать контекст (отладка)
        </label>
        </div>

        <div class="section">
        <button id="refactorBtn">🚀 Запустить рефакторинг</button>
        </div>

        <div id="status" class="status" style="display: none;"></div>

        <div id="resultSection" class="section" style="display: none;">
        <div class="section-title">💡 Результат</div>
        <div id="result" class="result"></div>
        </div>

        <div id="contextSection" class="section" style="display: none;">
        <div class="section-title collapsible" id="contextTitle">🔍 Контекст (для отладки) ▼</div>
        <div id="contextContent" class="collapsible-content context-area"></div>
        </div>

        <script>
        const vscode = acquireVsCodeApi();
        let currentFilePath = '';
        let currentLine = 0;
        let currentColumn = 0;

        document.getElementById('refactorBtn').addEventListener('click', () => {
            const goal = document.getElementById('goal').value;
            const showDebug = document.getElementById('showDebug').checked;

            if (!currentFilePath) {
                alert('Установите курсор в C++ файл');
                return;
            }

            vscode.postMessage({
                command: 'refactor',
                filePath: currentFilePath,
                line: currentLine,
                column: currentColumn,
                goal: goal,
                showDebug: showDebug
            });

            document.getElementById('status').style.display = 'block';
        document.getElementById('status').className = 'status processing';
        document.getElementById('status').innerHTML = '⏳ Анализ...';
        document.getElementById('resultSection').style.display = 'none';
        document.getElementById('contextSection').style.display = 'none';
        });

        document.getElementById('contextTitle').addEventListener('click', () => {
            const content = document.getElementById('contextContent');
            content.classList.toggle('open');
        });

        window.addEventListener('message', event => {
            const message = event.data;

            switch (message.command) {
                case 'setPosition':
                    currentFilePath = message.filePath;
                    currentLine = message.line;
                    currentColumn = message.column;
                    document.getElementById('fileInfo').innerHTML = message.filePath.split('/').pop();
                    document.getElementById('positionInfo').innerHTML = \`строка \${message.line}, колонка \${message.column}\`;
                    break;

                case 'status':
                    document.getElementById('status').innerHTML = '⏳ ' + message.message;
                    break;

                case 'result':
                    document.getElementById('status').style.display = 'none';
        document.getElementById('resultSection').style.display = 'block';
        document.getElementById('result').innerHTML = message.suggestion.replace(/\\n/g, '<br>').replace(/ /g, '&nbsp;');

        if (message.showDebug && message.sentContext) {
            document.getElementById('contextSection').style.display = 'block';
        document.getElementById('contextContent').innerHTML = message.sentContext.replace(/\\n/g, '<br>').replace(/ /g, '&nbsp;');
        document.getElementById('contextContent').classList.add('open');
        }
        break;

                case 'error':
                    document.getElementById('status').className = 'status error';
        document.getElementById('status').innerHTML = '❌ ' + message.message;
        break;
            }
        });

        vscode.postMessage({ command: 'getCurrentPosition' });

        setInterval(() => {
            vscode.postMessage({ command: 'getCurrentPosition' });
        }, 1000);
        </script>
        </body>
        </html>`;
    }
}
