"""Local UI fixture. Simulated responses only; never connects to hardware."""
import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
root = Path(__file__).resolve().parents[1]
latest = {}
class Handler(BaseHTTPRequestHandler):
    def reply(self, body, mime='application/json', status=200):
        data = body.encode('utf-8')
        self.send_response(status); self.send_header('Content-Type', mime)
        self.send_header('Content-Length', str(len(data))); self.end_headers(); self.wfile.write(data)
    def do_GET(self):
        if self.path == '/api/info':
            self.reply(json.dumps({'product':'UI preview (simulated)', 'fw_ver':'preview', 'imu_present':False, 'baud':1000000}))
        elif self.path == '/api/chassis': self.reply(json.dumps({'T':808,'ok':True,'ready':True,'enabled':False,'state':'disabled','led':0,'max_speed':0.32,'min_duty':.4,'timeout_ms':3000,'current':200,'can_id':1,'sta_rssi':-54,'ap_clients':1,'tilt':{'valid':True,'roll':0,'pitch':0},'wheels':[{'id':1,'valid':True,'voltage':20.1,'mps':0,'temperature_c':36,'current_valid':True,'current_raw':123},{'id':2,'valid':True,'voltage':20.1,'mps':0,'temperature_c':36,'current_valid':True,'current_raw':123}]}))
        elif self.path == '/api/imu': self.reply(json.dumps({'ok':False,'present':False,'error':'imu_not_present'}))
        elif self.path == '/api/result': self.reply(json.dumps(latest))
        elif self.path == '/ws': self.reply('{}', status=404)
        else: self.reply((root / 'web/index.html').read_text(encoding='utf-8'), 'text/html; charset=utf-8')
    def do_POST(self):
        global latest
        command = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        latest = {'T':command['T'],'rid':command.get('rid'),'ok':True,'simulated':True}
        if command['T'] == 700: latest['ids'] = [1, 5, 40]
        if command['T'] == 740: latest.update(ok=False, present=False, error='imu_not_present')
        if command['T'] in (723,732): latest.update(pos=2048,speed=0)
        if command['T'] == 713: latest.update(channels=[992]*16,sbus_status=0)
        self.reply('{"accepted":true}',status=202)
    def log_message(self, *_): pass
print('UI fixture: http://127.0.0.1:8765 (simulated data)', flush=True)
ThreadingHTTPServer(('127.0.0.1', 8765), Handler).serve_forever()
