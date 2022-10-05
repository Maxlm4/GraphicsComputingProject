#version 140
precision mediump float;

in vec3 vPosition; //Depending who compiles, these variables are not "attribute" but "in". In this version (130) both are accepted. in should be used later
in vec3 vNormal;
in vec2 vUV;

uniform mat4 uMVP;
uniform mat4 uModelView;

out vec3 varyNormal;
out vec3 varyPosition;
out vec2 vary_UV;

//We still use varying because OpenGLES 2.0 (OpenGL Embedded System, for example for smartphones) does not accept "in" and "out"

void main()
{
	gl_Position = uMVP * vec4(vPosition, 1.0); //We need to put vPosition as a vec4. Because vPosition is a vec3, we need one more value (w) which is here 1.0. Hence x and y go from -w to w hence -1 to +1. Premultiply this variable if you want to transform the position.
	varyNormal = normalize(transpose(inverse(mat3(uModelView))) * vNormal);
	vec4 worldPosition = uModelView * vec4(vPosition, 1.0);
	varyPosition = worldPosition.xyz / worldPosition.w;
	vary_UV = -vUV + vec2(1.0, 0.0);
}
