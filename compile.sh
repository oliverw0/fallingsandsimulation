#!/bin/sh
echo "Compiling into ./sand..."
if [ "$(uname)" = "Darwin" ]; then
  RL="$(brew --prefix raylib 2>/dev/null || echo /opt/homebrew)"
  clang++ -std=c++17 -O2 *.cpp -o sand -I"$RL/include" -L"$RL/lib" -lraylib -lm \
    -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -framework CoreAudio
else
  g++ -std=c++17 -O2 *.cpp -o sand -lraylib -lm
fi
echo "Done!"
