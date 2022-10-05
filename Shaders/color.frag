#version 140
precision mediump float; //Medium precision for float. highp and smallp can also be used

uniform vec4 uK;
uniform vec3 uColor;
uniform vec3 uLightPosition;
uniform vec3 uLightColor;
uniform vec3 uCameraPosition;
uniform sampler2D uTexture;

varying vec3 varyNormal; //Sometimes we use "out" instead of "varying". "out" should be used in later version of GLSL.
varying vec3 varyPosition;
varying vec2 vary_UV;


//We still use varying because OpenGLES 2.0 (OpenGL Embedded System, for example for smartphones) does not accept "in" and "out"

void main()
{
    vec3 L = normalize(uLightPosition-varyPosition);//light
    vec3 V = normalize(uCameraPosition-varyPosition);
	vec3 R = reflect(-L,varyNormal);
	vec3 texture = vec3(texture2D(uTexture, vary_UV));

    vec3 ambient = uK.x*texture*uLightColor;
    vec3 diffuse = uK.y*max(0.f,dot(varyNormal,L))*texture*uLightColor;
    vec3 specular = uK.z*pow(max(0.f,dot(R, V)), uK.w)*uLightColor;
    

    gl_FragColor = vec4(min(vec3(1.0,1.0,1.0), ambient + diffuse + specular),1.f);
}
