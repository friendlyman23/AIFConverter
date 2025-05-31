This program converts an .aif file to a .wav file, but unlike general-purpose
audio tools such as Audacity, it preserves important metadata that allow
samplers to identify a file's pitch, loop points, and other such information
critical for sampled audio to function as input into a digital instrument.

Currently, it supports only conversion of .aif files to the .wav format, and
only on Windows.

For testers, if you don't have an .aif file of your own, the run.bat script will 
automatically convert an .aif file I have placed in the data directory, and
dump the corresponding .wav file in that same directory. Otherwise, use by placing
the .aif file you wish to convert in the same directory as win32_converter.exe, 
then passing the filename to the executable as an argument on the command line.
