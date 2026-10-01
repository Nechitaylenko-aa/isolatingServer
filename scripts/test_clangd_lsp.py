import subprocess, json, time, sys

def rpc(msg):
    body = json.dumps(msg)
    return ('Content-Length: ' + str(len(body)) + '\r\n\r\n' + body).encode()

def read_msg(fp):
    hdr = b''
    while True:
        c = fp.read(1)
        if not c: return None
        hdr += c
        if hdr.endswith(b'\r\n\r\n'): break
    cl = 0
    for line in hdr.split(b'\r\n'):
        if line.lower().startswith(b'content-length:'):
            cl = int(line.split(b':')[1].strip())
    if cl <= 0: return None
    return json.loads(fp.read(cl))

proc = subprocess.Popen(
    ['clangd',
     '--compile-commands-dir=/home/artem/projects/databases/build',
     '--background-index=false', '--log=error',
     '--limit-results=0', '--offset-encoding=utf-8'],
    stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL
)

root = 'file:///home/artem/projects/databases'
furi = 'file:///home/artem/projects/databases/models/CDataBaseModel.h'

proc.stdin.write(rpc({'jsonrpc':'2.0','id':1,'method':'initialize','params':{
    'processId': None,
    'rootUri': root,
    'capabilities': {},
    'initializationOptions': {'compilationDatabasePath': '/home/artem/projects/databases/build'}
}}))
proc.stdin.flush()

for _ in range(30):
    m = read_msg(proc.stdout)
    if m and m.get('id') == 1:
        print('initialize: ok')
        break

proc.stdin.write(rpc({'jsonrpc':'2.0','method':'initialized','params':{}}))

# Open the header file
content = open('/home/artem/projects/databases/models/CDataBaseModel.h').read()
proc.stdin.write(rpc({'jsonrpc':'2.0','method':'textDocument/didOpen','params':{'textDocument':{
    'uri': furi, 'languageId': 'cpp', 'version': 1, 'text': content
}}}))

# Open all .cpp files from compile_commands.json
import json as _json
cc = _json.load(open('/home/artem/projects/databases/build/compile_commands.json'))
print(f'opening {len(cc)} cpp files...')
for entry in cc:
    src = entry.get('file', '')
    if not src: continue
    src_uri = 'file://' + src
    if src_uri == furi: continue
    try:
        src_content = open(src).read()
        proc.stdin.write(rpc({'jsonrpc':'2.0','method':'textDocument/didOpen','params':{'textDocument':{
            'uri': src_uri, 'languageId': 'cpp', 'version': 1, 'text': src_content
        }}}))
    except: pass
proc.stdin.flush()

print('waiting for publishDiagnostics for header...')
header_ready = False
for i in range(len(cc) * 6 + 40):
    m = read_msg(proc.stdout)
    if m is None:
        print('EOF after', i, 'messages')
        break
    method = m.get('method', '')
    if method == 'textDocument/publishDiagnostics':
        uri = m.get('params', {}).get('uri', '')
        if uri == furi:
            print(f'  msg[{i}]: header parsed!')
            header_ready = True
            break
        else:
            print(f'  msg[{i}]: publishDiagnostics for {uri.split("/")[-1]}')
    else:
        print(f'  msg[{i}]: {method or ("result id=" + str(m.get("id")))}')

if not header_ready:
    print('header not yet parsed, sending anyway')

proc.stdin.write(rpc({'jsonrpc':'2.0','id':2,'method':'textDocument/implementation','params':{
    'textDocument': {'uri': furi},
    'position': {'line': 75, 'character': 12}
}}))
proc.stdin.flush()
print('sent implementation request')

for i in range(80):
    m = read_msg(proc.stdout)
    if m is None:
        print('EOF')
        break
    if m.get('id') == 2:
        result = m.get('result')
        print('RESULT type:', type(result).__name__, 'len:', len(result) if isinstance(result, list) else 'n/a')
        print(json.dumps(result, indent=2, ensure_ascii=False)[:2000])
        break
    print(f'  skip[{i}]: {m.get("method","?")}')

proc.terminate()
proc.wait()