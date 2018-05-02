burro-engine
============

This is the README for the Burro Engine version 0.0. 

This is an experiment where I tried to write an interactive fiction
game engine rather like Twine, except using GTK3 instead of the browser
as the rendering engine.

To build it and run it from source, do the following commands

git clone https://github.com/spk121/burro
cd burro
./bootstrap.sh
./configure
make
./run-uninstalled.sh

Burro Engine uses an embedded Guile interpreter: the game scripts
are written in a language called Scheme.

It builds and runs, but, it is definitely an alpha.  See the TODO.org
for information on what needs to be done.

It comes with a default story, called "Fancy Free".  The game is
"game/game.burro" which you can run with the "run-uninstalled.sh"
script or by selecting game.burro from the File Open in the menu.
