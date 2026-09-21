import * as vscode from 'vscode';
import * as child_process from 'child_process';
import * as path from 'path';

let currentPanel: vscode.WebviewPanel | undefined;

export function activate(context: vscode.ExtensionContext) {
    const command = vscode.commands.registerCommand('cpp-ai-refactor.refactor', async () => {
        const editor = vscode.window.activeTextEditor;
        if (!editor || (editor.document.languageId !== 'cpp' && editor.document.languageId !== 'c')) {
            vscode.window.showErrorMessage('Откройте C или C++ файл (.c, .cpp, .h, .hpp, .hxx)');
            return;
        }

        const position = editor.selection.active;
        const filePath = editor.document.uri.fsPath;
        const line = position.line + 1;
        const column = position.character + 1;

        showPanel(context, filePath, line, column);
    });

    context.subscriptions.push(command);
}

function escapeHtml(text: string): string {
    return text
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;');
}

function escapeRegex(str: string): string {
    return str.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

function extractBodyFromSuggestion(suggestion: string): string | null {
    // Ищем блок ```cpp ... ```
    const cppBlockRegex = /```cpp\n([\s\S]*?)\n```/;
    const match = suggestion.match(cppBlockRegex);

    if (match && match[1]) {
        const fullCode = match[1];
        // Извлекаем тело между { и }
        const bracePos = fullCode.indexOf('{');
        if (bracePos !== -1) {
            return fullCode.substring(bracePos);
        }
        return fullCode;
    }

    // Если нет ```cpp, ищем просто ```
    const genericBlockRegex = /```\n([\s\S]*?)\n```/;
    const genericMatch = suggestion.match(genericBlockRegex);
    if (genericMatch && genericMatch[1]) {
        const fullCode = genericMatch[1];
        const bracePos = fullCode.indexOf('{');
        if (bracePos !== -1) {
            return fullCode.substring(bracePos);
        }
        return fullCode;
    }

    return null;
}

async function applyChanges(filePath: string, bodyStartLine: number, bodyStartCol: number,
                            bodyEndLine: number, bodyEndCol: number, newBody: string) {
    const document = await vscode.workspace.openTextDocument(filePath);
    const editor = await vscode.window.showTextDocument(document);

    // libclang возвращает 1-based, vscode использует 0-based
    const start = new vscode.Position(bodyStartLine - 1, bodyStartCol);
    const end = new vscode.Position(bodyEndLine - 1, bodyEndCol);

    // Получаем отступ из оригинального кода
    const startLineText = document.lineAt(bodyStartLine - 1).text;
    const indent = startLineText.match(/^\s*/)?.[0] || '';

    // Форматируем новое тело
    const formattedBody = newBody.split('\n').map((line, i) => {
        if (i === 0) return line;
        return indent + line;
    }).join('\n');

    const edit = new vscode.WorkspaceEdit();
    edit.replace(document.uri, new vscode.Range(start, end), formattedBody);

    const success = await vscode.workspace.applyEdit(edit);
    if (success) {
        vscode.window.showInformationMessage('Метод успешно обновлён');
    } else {
        vscode.window.showErrorMessage('Не удалось обновить метод');
    }

    return success;
}

function showPanel(context: vscode.ExtensionContext, filePath: string, line: number, column: number) {
    if (currentPanel) {
        currentPanel.reveal(vscode.ViewColumn.Beside);
    } else {
        currentPanel = vscode.window.createWebviewPanel(
            'cppRefactor',
            'C++ AI Refactor',
            vscode.ViewColumn.Beside,
            {
                enableScripts: true,
                retainContextWhenHidden: true
            }
        );
        currentPanel.onDidDispose(() => { currentPanel = undefined; });
    }

    currentPanel.webview.html = getHtml(filePath, line, column);

    currentPanel.webview.onDidReceiveMessage(async (msg) => {
        if (msg.command === 'refactor') {
            await runRefactor(context, msg.filePath, msg.line, msg.column, msg.goal);
        } else if (msg.command === 'copySuggestion') {
            vscode.env.clipboard.writeText(msg.text);
            vscode.window.showInformationMessage('Предложение скопировано в буфер обмена');
        } else if (msg.command === 'applyChanges') {
            const editor = vscode.window.activeTextEditor;
            if (!editor) {
                vscode.window.showErrorMessage('Нет активного редактора');
                return;
            }

            const newBody = extractBodyFromSuggestion(msg.suggestion);
            if (!newBody) {
                vscode.window.showErrorMessage('Не удалось извлечь код из предложения');
                return;
            }

            //await applyChanges(msg.filePath, msg.line, msg.signature, newBody);
            await applyChanges(
                msg.filePath,
                msg.bodyStartLine,
                msg.bodyStartCol,
                msg.bodyEndLine,
                msg.bodyEndCol,
                msg.newBody
            );
        }
    });
}

async function runRefactor(context: vscode.ExtensionContext, filePath: string, line: number, column: number, goal: string) {
    const toolPath = path.join(context.extensionPath, 'cpp-tool', 'analyzer');

    vscode.window.showInformationMessage(`Запуск анализа: ${filePath}:${line}:${column}`);

    currentPanel?.webview.postMessage({
        command: 'status',
        status: 'loading',
        message: '🔍 Анализ кода...'
    });

    try {
        const result = await new Promise<string>((resolve, reject) => {
            const cmd = `"${toolPath}" "${filePath}" ${line} ${column} "${goal}"`;
            vscode.window.showInformationMessage(`Выполняется: ${toolPath}`);

            child_process.execFile(toolPath, [filePath, String(line), String(column), goal],
                                   { maxBuffer: 1024 * 1024 * 10 },
                                   (err, stdout, stderr) => {
                                       if (err) {
                                           vscode.window.showErrorMessage(`Ошибка: ${err.message}\n${stderr}`);
                                           reject(err);
                                       } else {
                                           vscode.window.showInformationMessage(`Получен ответ, длина: ${stdout.length}`);
                                           resolve(stdout);
                                       }
                                   });
        });

        vscode.window.showInformationMessage(`Парсинг JSON...`);

        let data;
        try {
            data = JSON.parse(result);
            vscode.window.showInformationMessage(`JSON успешно распарсен, метод: ${data.method}`);
        } catch (e) {
            vscode.window.showErrorMessage(`Ошибка парсинга JSON: ${e}\nПервые 200 символов: ${result.substring(0, 200)}`);
            return;
        }

        currentPanel?.webview.postMessage({
            command: 'result',
            suggestion: data.suggestion || 'Нет предложений',
            context: data.context || 'Контекст не получен',
            method: data.method,
            className: data.class,
            signature: data.signature || '',
            filePath: filePath,
            line: line,
            bodyStartLine: data.body_start_line,
            bodyStartCol: data.body_start_col,
            bodyEndLine: data.body_end_line,
            bodyEndCol: data.body_end_col
        });

        vscode.window.showInformationMessage(`Результат отправлен в webview`);

    } catch (err: any) {
        vscode.window.showErrorMessage(`❌ Ошибка: ${err.message}`);
        currentPanel?.webview.postMessage({
            command: 'status',
            status: 'error',
            message: `❌ Ошибка: ${err.message}`
        });
        console.error('Refactor error:', err);
    }
}

function getHtml(filePath: string, line: number, column: number): string {
    return `<!DOCTYPE html>
    <html>
    <head>
    <style>
    body {
        padding: 20px;
        font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, monospace;
        background-color: #1e1e1e;
        color: #d4d4d4;
    }
    .header {
        background-color: #2d2d30;
        padding: 10px;
        border-radius: 5px;
        margin-bottom: 15px;
        font-size: 12px;
    }
    .file-info { color: #9cdcfe; }
    .line-info { color: #ce9178; }
    .input-section { margin: 15px 0; }
    label {
        display: block;
        margin-bottom: 5px;
        color: #9cdcfe;
        font-weight: bold;
    }
    textarea {
        width: 100%;
        background-color: #2d2d30;
        color: #d4d4d4;
        border: 1px solid #3e3e42;
        border-radius: 3px;
        padding: 8px;
        font-family: monospace;
        resize: vertical;
    }
    button {
        background-color: #0e639c;
        color: white;
        border: none;
        padding: 8px 16px;
        border-radius: 3px;
        cursor: pointer;
        font-size: 14px;
        margin-top: 10px;
    }
    button:hover { background-color: #1177bb; }
    button:disabled {
        background-color: #3e3e42;
        cursor: not-allowed;
    }
    .status {
        margin: 10px 0;
        padding: 10px;
        border-radius: 3px;
        display: none;
    }
    .status.loading {
        display: block;
        background-color: #2d2d30;
        border-left: 4px solid #007acc;
        color: #9cdcfe;
    }
    .status.error {
        display: block;
        background-color: #2d2d30;
        border-left: 4px solid #f48771;
        color: #f48771;
    }
    .tabs {
        display: flex;
        margin: 15px 0;
        border-bottom: 1px solid #3e3e42;
    }
    .tab {
        padding: 8px 16px;
        cursor: pointer;
        background: none;
        border: none;
        color: #d4d4d4;
        font-size: 14px;
    }
    .tab:hover { background-color: #2d2d30; }
    .tab.active {
        color: #9cdcfe;
        border-bottom: 2px solid #9cdcfe;
    }
    .content {
        display: none;
        padding: 10px;
        background-color: #252526;
        border-radius: 3px;
        overflow-x: auto;
        max-height: 500px;
        overflow-y: auto;
    }
    .content.active { display: block; }
    .content pre {
        margin: 0;
        white-space: pre-wrap;
        font-family: 'Consolas', monospace;
        font-size: 13px;
        line-height: 1.5;
    }
    .action-buttons {
        margin-top: 10px;
        display: flex;
        gap: 10px;
    }
    .action-buttons button {
        margin-top: 0;
        font-size: 12px;
        padding: 4px 12px;
    }
    .action-buttons button.secondary {
        background-color: #3e3e42;
    }
    .spinner {
        display: inline-block;
        width: 12px;
        height: 12px;
        border: 2px solid #9cdcfe;
        border-top-color: transparent;
        border-radius: 50%;
        animation: spin 0.8s linear infinite;
        margin-right: 8px;
    }
    @keyframes spin {
        to { transform: rotate(360deg); }
    }
    </style>
    </head>
    <body>
    <div class="header">
    <div>📁 <span class="file-info">${escapeHtml(filePath)}</span></div>
    <div>📍 <span class="line-info">Строка ${line}, Колонка ${column}</span></div>
    </div>

    <div class="input-section">
    <label>🎯 Цель рефакторинга:</label>
    <textarea id="goal" rows="3">Добавить проверки на nullptr и обработку ошибок</textarea>
    <button id="run">🚀 Запустить рефакторинг</button>
    </div>

    <div id="status" class="status"></div>

    <div class="tabs" style="display: none;" id="tabs">
    <button class="tab active" data-tab="suggestion">💡 Предложение</button>
    <button class="tab" data-tab="context">🔍 Контекст (Debug)</button>
    </div>

    <div id="suggestion" class="content"></div>
    <div id="context" class="content"></div>

    <script>
    const vscode = acquireVsCodeApi();
    let currentSuggestion = '';
    let currentContext = '';
    let currentMethod = '';
    let currentSignature = '';
    let currentFilePath = '';
    let currentLine = 0;

    document.querySelectorAll('.tab').forEach(tab => {
        tab.addEventListener('click', () => {
            const tabId = tab.dataset.tab;
            document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
            tab.classList.add('active');
            document.querySelectorAll('.content').forEach(c => c.classList.remove('active'));
            document.getElementById(tabId).classList.add('active');
        });
    });

    document.getElementById('run').onclick = () => {
        const goal = document.getElementById('goal').value;
        if (!goal.trim()) {
            showStatus('error', '❌ Введите цель рефакторинга');
            return;
        }

        document.getElementById('suggestion').innerHTML = '';
        document.getElementById('context').innerHTML = '';
        document.getElementById('tabs').style.display = 'none';

        const runBtn = document.getElementById('run');
        runBtn.disabled = true;
        runBtn.textContent = '⏳ Обработка...';

        vscode.postMessage({
            command: 'refactor',
            filePath: '${escapeHtml(filePath)}',
                           line: ${line},
                           column: ${column},
                           goal: goal
        });
    };

    function showStatus(type, message) {
        const statusDiv = document.getElementById('status');
        statusDiv.className = 'status ' + type;
        statusDiv.innerHTML = message;
        if (type !== 'loading') {
            setTimeout(() => {
                if (statusDiv.className === 'status ' + type) {
                    statusDiv.style.display = 'none';
                }
            }, 5000);
        }
        statusDiv.style.display = 'block';
    }

    window.addEventListener('message', e => {
        const data = e.data;

        if (data.command === 'status') {
            if (data.status === 'loading') {
                showStatus('loading', '<span class="spinner"></span>' + data.message);
            } else if (data.status === 'error') {
                showStatus('error', data.message);
                document.getElementById('run').disabled = false;
                document.getElementById('run').textContent = '🚀 Запустить рефакторинг';
            }
        } else if (data.command === 'result') {
            currentSuggestion = data.suggestion;
            currentContext = data.context;
            currentMethod = data.method;
            currentSignature = data.signature;
            currentFilePath = data.filePath;
            currentLine = data.line;

            const suggestionDiv = document.getElementById('suggestion');
            suggestionDiv.innerHTML = '<pre>' + escapeHtml(currentSuggestion) + '</pre>';

    const actionButtons = document.createElement('div');
    actionButtons.className = 'action-buttons';
    actionButtons.innerHTML = \`
    <button id="copySuggestion">📋 Копировать предложение</button>
    <button id="applyChanges" class="secondary">✏️ Применить изменения</button>
    \`;
    suggestionDiv.appendChild(actionButtons);

    document.getElementById('copySuggestion').onclick = () => {
        vscode.postMessage({
            command: 'copySuggestion',
            text: currentSuggestion
        });
    };

    document.getElementById('applyChanges').onclick = () => {
        vscode.postMessage({
            command: 'applyChanges',
            suggestion: currentSuggestion,
            signature: currentSignature,
            filePath: currentFilePath,
            line: currentLine
        });
    };

    const contextDiv = document.getElementById('context');
    contextDiv.innerHTML = '<pre>' + escapeHtml(currentContext) + '</pre>';

    document.getElementById('tabs').style.display = 'flex';
    document.getElementById('suggestion').classList.add('active');

    document.getElementById('run').disabled = false;
    document.getElementById('run').textContent = '🚀 Запустить рефакторинг';
    document.getElementById('status').style.display = 'none';
        }
    });

    function escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
    </script>
    </body>
    </html>`;
}

export function deactivate() {}
