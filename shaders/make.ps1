rm ../build/shaders/*
glslangValidator -V shader.vert
glslangValidator -V shader.frag
mv *.spv ../build/shaders/
