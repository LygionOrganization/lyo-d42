from pathlib import Path
import os,subprocess
root=Path(__file__).resolve().parents[1]
out=root/'.pio/chassis_tests';out.mkdir(parents=True,exist_ok=True)
vs=Path(os.environ.get('ProgramFiles(x86)','C:/Program Files (x86)'))/'Microsoft Visual Studio'
vc=next(vs.glob('*/BuildTools/VC/Auxiliary/Build/vcvars64.bat'))
incs=[root/'test/chassis_stubs',root/'include',root/'src',next((root/'.pio/libdeps').glob('*/ArduinoJson/src'))]
import sys
source='can_test.cpp' if '--can' in sys.argv else 'chassis_test.cpp'
if '--screen' in sys.argv:
    source='screen_test.cpp'
    incs.insert(0,root/'test/screen_stubs')
batch=out/'run.cmd'
batch.write_text(f'call "{vc}" >nul\ncl /nologo /std:c++17 /EHsc /W4 '+ ' '.join(f'/I"{p}"' for p in incs)+f' "{root / "test" / source}" /Fe:tests.exe\nif errorlevel 1 exit /b 1\ntests.exe\n',encoding='utf-8')
subprocess.run(['cmd.exe','/c',str(batch)],cwd=out,check=True)
