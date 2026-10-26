#!/usr/bin/env python3
"""Reproduce observed Claude policies; this is not an ESP32 build of that project."""
from pathlib import Path
import re, subprocess, tempfile
root = Path(__file__).resolve().parents[1]
original = (root/'reference/CLAUDE_ORIGINAL.md').read_text()
files = dict(re.findall(r'^## ([^\n]+)\n\n```[^\n]*\n(.*?)\n```', original, re.M | re.S))
harness = '#include <cassert>\n#include <cstdio>\n#include "logic.h"\n#include "feedback.cpp"\nfloat activeObstacleThresholdCM=50, activeStairThresholdCM=170;\nuint32_t activeVibDuty=255;\nchar activeProfileName[13]="DEFAULT";\nint main() {\n  float cm;\n  assert(nav::classifyEcho(false,0,true,true,cm)==nav::EchoResult::NoObstacle);\n  puts("Claude: HIGH remaining at timeout is classified as normal NoObstacle");\n  nav::UpperObstacle upper;\n  upper.sample(119); upper.sample(125); upper.sample(119); assert(upper.active);\n  puts("Claude: 119/125/119cm triggers despite nonconsecutive below-120 samples");\n  nav::FallDetector fall;\n  fall.sample(2,0); fall.sample(20,601); fall.sample(9.8,1101);\n  assert(fall.sample(9.8,2101));\n  puts("Claude: impact at 601ms after freefall is accepted as a fall sequence");\n  feedbackBegin(); fakeMillis=0; feedbackStart(Feedback::Fall);\n  fakeMillis=700; feedbackTick(true,-1,-1,false,-1);\n  assert(fakeDuty[cfg::VIB_CH_MAIN]==activeVibDuty);\n  puts("Claude: stair alert keeps central motor ON during the fall pattern OFF interval");\n}\n'
with tempfile.TemporaryDirectory() as directory:
    temp=Path(directory)
    for name in ('config.h','logic.h','feedback.cpp','feedback.h','profiles.h'):
        (temp/name).write_text(files['src/'+name])
    source=temp/'policy.cpp'; source.write_text(harness)
    binary=temp/'policy'
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',
                    '-I',str(root/'test/stubs'),str(source),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True,timeout=5)
