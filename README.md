# bobblehead
A bob-based beat tracker

This is just a small proof of concept of a beat tracking system based on
the visual input of a person bobbing the head with the rhythm of the music.
The app is built with (openFrameworks)[1] and uses the (ofxCv)[2] addon to
interact with (OpenCV)[3].

The logic of the program is based on the idea of following the head of the
person using Viola-Jones object detection and identifying sudden changes in
the velocity. There is also some Kalman filtering to smoothen things a bit.
When some reasonable beat duration is inferred, the tempo is shown on the
screen. Slow tempos are somewhat tracked, faster tempos not so much :(

The program does not require any special controls, but you can press key `d`
to hide the visual tracking information. Pressing `m` toggles a visual
metronome that does not work nearly as well as it should; beat phase seems
to be harder to grasp.
