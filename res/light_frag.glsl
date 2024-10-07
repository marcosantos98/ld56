#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Output fragment color
out vec4 finalColor;

// NOTE: Add here your custom variables
struct Light {
	vec2 pos;
	float radius;
	vec4 color;
};

#define MAX_LIGHTS 100
uniform Light lights[MAX_LIGHTS];

void main()
{
	vec2 fragTexCoords = gl_FragCoord.xy; 
	vec4 texColor = texture(texture0, fragTexCoord);
    vec4 color = vec4(0.0, 0, 0, 1); // Start with black (dark)

    // Calculate illumination from lights
    for (int i = 0; i < MAX_LIGHTS; i++) {
		if (lights[i].radius > 0) {
			float dist = distance(fragTexCoords, lights[i].pos);
        	if (dist < lights[i].radius) {
        		float alpha = 1.0 - (dist / lights[i].radius);
        		color -= lights[i].color * alpha; // Accumulate light
        	}
		}
    }

    // Blend the texture color with the illuminated color
    finalColor = mix(texColor, color, color.a);
}
