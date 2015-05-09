/****************************************************************************/
/*	Render water surface using Gerstner waves	*/
/*	with textures, VAO and normal calculating.	*/
/*	Author:	John Hany	*/
/*	Updated version can be downloaded from:		*/
/*		https://github.com/johnhany/OpenGLProjects	*/
/*	You can read my blog about the code in:		*/
/*		http://johnhany.net/2014/02/water-rendering-with-gerstner-wave/	*/

#include <stdafx.h>
#include <stdio.h>
#include <stdlib.h>
#include <gl/glew.h>
#ifdef __APPLE__
#  include <gl/glut.h>
#else
#  include <freeglut/freeglut.h>
#endif
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <math.h>
#include "util.h"

#define SCREEN_WIDTH	800
#define SCREEN_HEIGHT	600

#define START_X		-4.0
#define START_Y		-2.5
#define START_Z		0
#define LENGTH_X	0.1
#define LENGTH_Y	0.1

#define HEIGHT_SCALE	1.6

#define WAVE_COUNT		6

#define STRIP_COUNT		80
#define STRIP_LENGTH	50
#define DATA_LENGTH		STRIP_LENGTH*2*(STRIP_COUNT-1)

static GLfloat pt_strip[STRIP_COUNT*STRIP_LENGTH*3] = {0};
static GLfloat pt_normal[STRIP_COUNT*STRIP_LENGTH*3] = {0};
static GLfloat vertex_data[DATA_LENGTH*3] = {0};
static GLfloat normal_data[DATA_LENGTH*3] = {0};

//wave_length, wave_height, wave_dir, wave_speed, wave_start.x, wave_start.y
static const GLfloat wave_para[6][6] = {
	{	1.6,	0.12,	0.9,	0.06,	0.0,	0.0	},
	{	1.3,	0.1,	1.14,	0.09,	0.0,	0.0	},
	{	0.2,	0.01,	0.8,	0.08,	0.0,	0.0	},
	{	0.18,	0.008,	1.05,	0.1,	0.0,	0.0	},
	{	0.23,	0.005,	1.15,	0.09,	0.0,	0.0	},
	{	0.12,	0.003,	0.97,	0.14,	0.0,	0.0	}
};

static const GLfloat gerstner_pt_a[22] = {
	0.0,0.0, 41.8,1.4, 77.5,5.2, 107.6,10.9,
	132.4,17.7, 152.3,25.0, 167.9,32.4, 179.8,39.2,
	188.6,44.8, 195.0,48.5, 200.0,50.0
};
static const GLfloat gerstner_pt_b[22] = {
	0.0,0.0, 27.7,1.4, 52.9,5.2, 75.9,10.8,
	97.2,17.6, 116.8,25.0, 135.1,32.4, 152.4,39.2,
	168.8,44.8, 184.6,48.5, 200.0,50.0
};
static const GLint gerstner_sort[6] = {
	0, 0, 1, 1, 1, 1
};

static struct {
	GLuint vertex_buffer, normal_buffer;
	GLuint vertex_shader, fragment_shader, program;
	GLuint diffuse_texture, normal_texture;

	struct {
		GLint diffuse_texture, normal_texture;
	} uniforms;

	struct {
		GLint position;
		GLint normal;
	} attributes;
} names;

static struct {
	GLfloat time;
	GLfloat wave_length[WAVE_COUNT],
		wave_height[WAVE_COUNT],
		wave_dir[WAVE_COUNT],
		wave_speed[WAVE_COUNT],
		wave_start[WAVE_COUNT*2];
} values;

static void infoLog(GLuint, PFNGLGETSHADERIVPROC, PFNGLGETSHADERINFOLOGPROC);
void *readShader(const char *, GLint *);
static GLuint initShader(GLenum, const char *);
static void installShaders(void);
static void idleFunc(void);
static void calcuGerstnerGradient(const GLfloat *, GLfloat *);
static void initWave(void);
static float gerstnerZ(float, float, float, const GLfloat *);
static int normalizeF(float *, float *, int);
static void calcuWave(void);
static void initGL(void);
static void displayFunc(void);

int main(int argc, char** argv)
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(SCREEN_WIDTH, SCREEN_HEIGHT);
	glutCreateWindow("wave-wire-gerstner");
	glutIdleFunc(&idleFunc);
	glutDisplayFunc(&displayFunc);

	glewInit();
	if (!GLEW_VERSION_2_0) {
		fprintf(stderr, "OpenGL 2.0 not available\n");
		getchar();
	}
	installShaders();
	initWave();
	initGL();

	glutMainLoop();
	return 0;
}

static void infoLog(GLuint object, PFNGLGETSHADERIVPROC glGet__iv, PFNGLGETSHADERINFOLOGPROC glGet__InfoLog)
{
	GLint log_length;
	char *log;

	glGet__iv(object, GL_INFO_LOG_LENGTH, &log_length);
	log = (char *)malloc(log_length);
	glGet__InfoLog(object, log_length, NULL, log);
	fprintf(stderr, "%s", log);
	free(log);
}

void *readShader(const char *filename, GLint *length)
{
	FILE *f = fopen(filename, "r");
	void *buffer;

	if (!f) {
		fprintf(stderr, "Unable to open %s for reading\n", filename);
		return NULL;
	}

	fseek(f, 0, SEEK_END);
	*length = ftell(f);
	fseek(f, 0, SEEK_SET);

	buffer = malloc(*length+1);
	*length = fread(buffer, 1, *length, f);
	fclose(f);
	((char*)buffer)[*length] = '\0';

	return buffer;
}

static GLuint initShader(GLenum type, const char *filename)
{
	GLint length;
	GLchar *source = (GLchar *)readShader(filename, &length);
	GLuint shader;
	GLint shader_ok;

	if (!source)
		return 0;

	shader = glCreateShader(type);
	glShaderSource(shader, 1, (const GLchar**)&source, &length);
	free(source);
	glCompileShader(shader);

	glGetShaderiv(shader, GL_COMPILE_STATUS, &shader_ok);
	if (!shader_ok) {
		fprintf(stderr, "Failed to compile %s:\n", filename);
		infoLog(shader, glGetShaderiv, glGetShaderInfoLog);
		glDeleteShader(shader);
		getchar();
	}
	return shader;
}

static void installShaders(void)
{
	names.vertex_shader = initShader(GL_VERTEX_SHADER, "wave-vs.glsl");
	names.fragment_shader = initShader(GL_FRAGMENT_SHADER, "wave-fs.glsl");

	GLint program_ok;
	names.program = glCreateProgram();
	glAttachShader(names.program, names.vertex_shader);
	glAttachShader(names.program, names.fragment_shader);
	glLinkProgram(names.program);
	glGetProgramiv(names.program, GL_LINK_STATUS, &program_ok);
	if (!program_ok) {
		fprintf(stderr, "Failed to link shader program:\n");
		infoLog(names.program, glGetProgramiv, glGetProgramInfoLog);
		glDeleteProgram(names.program);
		getchar();
	}
	glUseProgram(names.program);
}

static GLuint initTexture(const char *filename)
{
	int width, height;
	void *pixels = read_tga(filename, &width, &height);
	GLuint texture;

	if (!pixels)
		return 0;

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, pixels);

	free(pixels);
	return texture;
}

static void initWave(void)
{
	//Initialize values{}
	values.time = 0.0;
	for(int w=0; w<WAVE_COUNT; w++)
	{
		values.wave_length[w] = wave_para[w][0];
		values.wave_height[w] = wave_para[w][1];
		values.wave_dir[w] = wave_para[w][2];
		values.wave_speed[w] = wave_para[w][3];
		values.wave_start[w*2] = wave_para[w][4];
		values.wave_start[w*2+1] = wave_para[w][5];
	}

	//Initialize pt_strip[]
	int index=0;
	for(int i=0; i<STRIP_COUNT; i++)
	{
		for(int j=0; j<STRIP_LENGTH; j++)
		{
			pt_strip[index] = START_X + i*LENGTH_X;
			pt_strip[index+1] = START_Y + j*LENGTH_Y;
			index += 3;
		}
	}
}

static float gerstnerZ(float w_length, float w_height, float x_in, const GLfloat gerstner[22])
{
	x_in = x_in * 400.0 / w_length;

	while(x_in < 0.0)
		x_in += 400.0;
	while(x_in > 400.0)
		x_in -= 400.0;
	if(x_in > 200.0)
		x_in = 400.0 - x_in;

	int i = 0;
	float yScale = w_height/50.0;
	while(i<18 && (x_in<gerstner[i] || x_in>=gerstner[i+2]))
		i+=2;
	if(x_in == gerstner[i])
		return gerstner[i+1] * yScale;
	if(x_in > gerstner[i])
		return ((gerstner[i+3]-gerstner[i+1]) * (x_in-gerstner[i]) / (gerstner[i+2]-gerstner[i]) + gerstner[i+3]) * yScale;
}

static int normalizeF(float in[], float out[], int count)
{
	int t=0;
	float l = 0.0;

	if(count <= 0.0){
		printf("normalizeF(): Number of dimensions should be larger than zero.\n");
		return 1;
	}
	while(t<count && in[t]<0.0000001 && in[t]>-0.0000001){
		t++;
	}
	if(t == count){
		printf("normalizeF(): The input vector is too small.\n");
		return 1;
	}
	for(t=0; t<count; t++)
		l += in[t] * in[t];
	if(l < 0.0000001){
		l = 0.0;
		for(t=0; t<count; t++)
			in[t] *= 10000.0;
		for(t=0; t<count; t++)
			l += in[t] * in[t];
	}
	l = sqrt(l);
	for(t=0; t<count; t++)
		out[t] /= l;

	return 0;
}

static void calcuWave(void)
{
	//Calculate pt_strip[z], poly_normal[] and pt_normal[]
	int index=0;
	float d, wave;
	for(int i=0; i<STRIP_COUNT; i++)
	{
		for(int j=0; j<STRIP_LENGTH; j++)
		{
			wave = 0.0;
			for(int w=0; w<WAVE_COUNT; w++){
				d = (pt_strip[index] - values.wave_start[w*2] + (pt_strip[index+1] - values.wave_start[w*2+1]) * tan(values.wave_dir[w])) * cos(values.wave_dir[w]);
				if(gerstner_sort[w] == 1){
					wave += values.wave_height[w] - gerstnerZ(values.wave_length[w], values.wave_height[w], d + values.wave_speed[w] * values.time, gerstner_pt_a);
				}else{
					wave += values.wave_height[w] - gerstnerZ(values.wave_length[w], values.wave_height[w], d + values.wave_speed[w] * values.time, gerstner_pt_b);
				}
			}
			pt_strip[index+2] = START_Z + wave*HEIGHT_SCALE;
			index += 3;
		}
	}

	index = 0;
	for(int i=0; i<STRIP_COUNT; i++)
	{
		for(int j=0; j<STRIP_LENGTH; j++)
		{
			int p0 = index-STRIP_LENGTH*3, p1 = index+3, p2 = index+STRIP_LENGTH*3, p3 = index-3;
			float xa, ya, za, xb, yb, zb;
			if(i > 0){
				if(j > 0){
					xa = pt_strip[p0] - pt_strip[index], ya = pt_strip[p0+1] - pt_strip[index+1], za = pt_strip[p0+2] - pt_strip[index+2];
					xb = pt_strip[p3] - pt_strip[index], yb = pt_strip[p3+1] - pt_strip[index+1], zb = pt_strip[p3+2] - pt_strip[index+2];
					pt_normal[index] += ya*zb-yb*za;
					pt_normal[index+1] += xb*za-xa*zb;
					pt_normal[index+2] += xa*yb-xb*ya;
				}
				if(j < STRIP_LENGTH-1){
					xa = pt_strip[p1] - pt_strip[index], ya = pt_strip[p1+1] - pt_strip[index+1], za = pt_strip[p1+2] - pt_strip[index+2];
					xb = pt_strip[p0] - pt_strip[index], yb = pt_strip[p0+1] - pt_strip[index+1], zb = pt_strip[p0+2] - pt_strip[index+2];
					pt_normal[index] += ya*zb-yb*za;
					pt_normal[index+1] += xb*za-xa*zb;
					pt_normal[index+2] += xa*yb-xb*ya;
				}
			}
			if(i < STRIP_COUNT-1){
				if(j > 0){
					xa = pt_strip[p3] - pt_strip[index], ya = pt_strip[p3+1] - pt_strip[index+1], za = pt_strip[p3+2] - pt_strip[index+2];
					xb = pt_strip[p2] - pt_strip[index], yb = pt_strip[p2+1] - pt_strip[index+1], zb = pt_strip[p2+2] - pt_strip[index+2];
					pt_normal[index] += ya*zb-yb*za;
					pt_normal[index+1] += xb*za-xa*zb;
					pt_normal[index+2] += xa*yb-xb*ya;
				}
				if(j < STRIP_LENGTH-1){
					xa = pt_strip[p2] - pt_strip[index], ya = pt_strip[p2+1] - pt_strip[index+1], za = pt_strip[p2+2] - pt_strip[index+2];
					xb = pt_strip[p1] - pt_strip[index], yb = pt_strip[p1+1] - pt_strip[index+1], zb = pt_strip[p1+2] - pt_strip[index+2];
					pt_normal[index] += ya*zb-yb*za;
					pt_normal[index+1] += xb*za-xa*zb;
					pt_normal[index+2] += xa*yb-xb*ya;
				}
			}
			if(normalizeF(&pt_normal[index], &pt_normal[index], 3))
				printf("%d\t%d\n", index/3/STRIP_LENGTH, (index/3)%STRIP_LENGTH);

			index += 3;
		}
	}

	//Calculate vertex_data[] according to pt_strip[], and normal_data[] according to pt_normal[]
	int pt;
	for(int c=0; c<(STRIP_COUNT-1); c++)
	{
		for(int l=0; l<2*STRIP_LENGTH; l++)
		{
			if(l%2 == 1){
				pt = c*STRIP_LENGTH + l/2;
			}else{
				pt = c*STRIP_LENGTH + l/2 + STRIP_LENGTH;
			}
			index = STRIP_LENGTH*2*c+l;
			for(int i=0; i<3; i++){
				vertex_data[index*3+i] = pt_strip[pt*3+i];
				normal_data[index*3+i] = pt_normal[pt*3+i];
			}
		}
	}
}

static void idleFunc(void)
{
	values.time += 0.05;
	glutPostRedisplay();
}

static void initGL(void)
{
	glClearColor(0.0, 0.0, 0.0, 1.0);

	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	GLfloat materAmbient[] = {0.1, 0.1, 0.3, 1.0};
	GLfloat materSpecular[] = {0.8, 0.8, 0.9, 1.0};
	GLfloat lightDiffuse[] = {0.7, 0.7, 0.8, 1.0};
	GLfloat lightAmbient[] = {0.0, 0.0, 0.0, 1.0};
	GLfloat lightSpecular[] = {1.0, 1.0, 1.0, 1.0};
	GLfloat envirAmbient[] = {0.1, 0.1, 0.3, 1.0};
	glUniform4fv(glGetUniformLocation(names.program, "materAmbient"), 1, materAmbient);
	glUniform4fv(glGetUniformLocation(names.program, "materSpecular"), 1, materSpecular);
	glUniform4fv(glGetUniformLocation(names.program, "lightDiffuse"), 1, lightDiffuse);
	glUniform4fv(glGetUniformLocation(names.program, "lightAmbient"), 1, lightAmbient);
	glUniform4fv(glGetUniformLocation(names.program, "lightSpecular"), 1, lightSpecular);
	glUniform4fv(glGetUniformLocation(names.program, "envirAmbient"), 1, envirAmbient);

	names.attributes.position = glGetAttribLocation(names.program, "position");
	glGenBuffers(1, &names.vertex_buffer);

	names.attributes.normal = glGetAttribLocation(names.program, "normal");
	glGenBuffers(1, &names.normal_buffer);

	names.diffuse_texture = initTexture("water-texture-2.tga");
	names.uniforms.diffuse_texture = glGetUniformLocation(names.program, "textures[0]");
	glUniform1i(names.uniforms.diffuse_texture, 0);

	names.normal_texture = initTexture("water-texture-2-normal.tga");
	names.uniforms.normal_texture = glGetUniformLocation(names.program, "textures[1]");
	glUniform1i(names.uniforms.normal_texture, 1);

	glm::mat4 Projection = glm::perspective(45.0f, (float)(SCREEN_WIDTH/SCREEN_HEIGHT), 1.0f, 100.f);
	glm::mat4 viewTransMat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -2.5f));
	glm::mat4 viewRotateMat = glm::rotate(viewTransMat, -45.0f, glm::vec3(1.0f, 0.0f, 0.0f));
	glm::mat4 ModelViewMat = glm::scale(viewRotateMat, glm::vec3(1.0f, 1.0f, 1.0f));
	glm::mat3 NormalMat = glm::transpose(glm::inverse(glm::mat3(ModelViewMat)));
	glUniformMatrix4fv(glGetUniformLocation(names.program, "modelViewMat"), 1, GL_FALSE, glm::value_ptr(ModelViewMat));
	glUniformMatrix4fv(glGetUniformLocation(names.program, "perspProjMat"), 1, GL_FALSE, glm::value_ptr(Projection));
	glUniformMatrix3fv(glGetUniformLocation(names.program, "normalMat"), 1, GL_FALSE, glm::value_ptr(NormalMat));
}

static void displayFunc(void)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	calcuWave();

	glUniform1f(glGetUniformLocation(names.program, "time"), values.time);

	glBindBuffer(GL_ARRAY_BUFFER, names.vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data, GL_STATIC_DRAW);
	glVertexAttribPointer(names.attributes.position, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*3, (void*)0);
	glEnableVertexAttribArray(names.attributes.position);

	glBindBuffer(GL_ARRAY_BUFFER, names.normal_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(normal_data), normal_data, GL_STATIC_DRAW);
	glVertexAttribPointer(names.attributes.normal, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*3, (void*)0);
	glEnableVertexAttribArray(names.attributes.normal);

	for(int c=0; c<(STRIP_COUNT-1); c++)
		glDrawArrays(GL_TRIANGLE_STRIP, STRIP_LENGTH*2*c, STRIP_LENGTH*2);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, names.normal_texture);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, names.diffuse_texture);

	glutSwapBuffers();
}

