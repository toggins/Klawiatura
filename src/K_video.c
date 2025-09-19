#include "K_game.h"
#include "K_log.h"

#include "K_video.h"

#define CHECK_GL_EXTENSION(ext)                                                                                        \
	do {                                                                                                           \
		if ((ext))                                                                                             \
			break;                                                                                         \
		FATAL("Missing OpenGL extension: " #ext "\nAt least OpenGL 3.3 with shader support is required.");     \
	} while (0)

static SDL_Window* window = NULL;
static SDL_GLContext gpu = NULL;

static GLuint shader = 0;
static Uniforms uniforms = {-1};

static GLuint blank_texture = 0;
static StTinyMap* textures = NULL;

static VertexBatch batch = {0};
static Surface* current_surface = NULL;

VideoState video_state = {0};

static const GLchar *const vertex_source =
#include "embeds/vertex.glsl"
	, *const fragment_source =
#include "embeds/fragment.glsl"
	;

void video_init(bool bypass_shader) {
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	// Window
	window = SDL_CreateWindow("Klawiatura", SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_OPENGL);
	if (window == NULL)
		FATAL("Window fail: %s", SDL_GetError());

	// OpenGL
	gpu = SDL_GL_CreateContext(window);
	if (gpu == NULL || !SDL_GL_MakeCurrent(window, gpu))
		FATAL("GPU fail: %s", SDL_GetError());

	SDL_GL_SetSwapInterval(0);

	int version = gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress);
	if (version == 0)
		FATAL("Failed to load OpenGL functions");

	INFO("GLAD version: %d.%d", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));
	if (!GLAD_GL_VERSION_3_3)
		FATAL("Unsupported OpenGL version\nAt least OpenGL 3.3 with framebuffer and shader support is "
		      "required.");

	if (bypass_shader)
		INFO("! Bypassing shader support");
	else {
		CHECK_GL_EXTENSION(GLAD_GL_ARB_shader_objects);
		CHECK_GL_EXTENSION(GLAD_GL_ARB_vertex_shader);
		CHECK_GL_EXTENSION(GLAD_GL_ARB_fragment_shader);
		CHECK_GL_EXTENSION(GLAD_GL_ARB_vertex_program);
		CHECK_GL_EXTENSION(GLAD_GL_ARB_fragment_program);
	}

	INFO("OpenGL vendor: %s", glGetString(GL_VENDOR));
	INFO("OpenGL version: %s", glGetString(GL_VERSION));
	INFO("OpenGL renderer: %s", glGetString(GL_RENDERER));
	INFO("OpenGL shading language version: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));

	// Blank texture
	glGenTextures(1, &blank_texture);
	glBindTexture(GL_TEXTURE_2D, blank_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, WHITE);

	// Vertex batch
	glGenVertexArrays(1, &batch.vao);
	glBindVertexArray(batch.vao);
	glEnableVertexArrayAttrib(batch.vao, VATT_POSITION);
	glVertexArrayAttribFormat(batch.vao, VATT_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 3);
	glEnableVertexArrayAttrib(batch.vao, VATT_COLOR);
	glVertexArrayAttribFormat(batch.vao, VATT_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(GLubyte) * 4);
	glEnableVertexArrayAttrib(batch.vao, VATT_UV);
	glVertexArrayAttribFormat(batch.vao, VATT_UV, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 2);

	batch.vertex_count = 0;
	batch.vertex_capacity = 3;
	batch.vertices = SDL_malloc(batch.vertex_capacity * sizeof(Vertex));
	if (batch.vertices == NULL)
		FATAL("batch.vertices fail");

	glGenBuffers(1, &batch.vbo);
	glBindBuffer(GL_ARRAY_BUFFER, batch.vbo);
	glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(Vertex) * batch.vertex_capacity), NULL, GL_DYNAMIC_DRAW);

	glEnableVertexAttribArray(VATT_POSITION);
	glVertexAttribPointer(VATT_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));

	glEnableVertexAttribArray(VATT_COLOR);
	glVertexAttribPointer(VATT_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex, color));

	glEnableVertexAttribArray(VATT_UV);
	glVertexAttribPointer(VATT_UV, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));

	batch.color[0] = batch.color[1] = batch.color[2] = batch.color[3] = 1;
	batch.stencil = 0;
	batch.texture = blank_texture;
	batch.alpha_test = 0.5f;

	batch.blend_src[0] = batch.blend_src[1] = GL_SRC_ALPHA;
	batch.blend_dest[0] = GL_ONE_MINUS_SRC_ALPHA;
	batch.blend_dest[1] = GL_ONE;
	batch.logic = GL_COPY;

	batch.filter = false;

	GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex_shader, 1, &vertex_source, NULL);
	glCompileShader(vertex_shader);

	GLint success;
	glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLchar error[1024];
		glGetShaderInfoLog(vertex_shader, sizeof(error), NULL, error);
		FATAL("Vertex shader fail: %s", error);
	}

	GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment_shader, 1, &fragment_source, NULL);
	glCompileShader(fragment_shader);

	glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLchar error[1024];
		glGetShaderInfoLog(fragment_shader, sizeof(error), NULL, error);
		FATAL("Fragment shader fail: %s", error);
	}

	shader = glCreateProgram();
	glAttachShader(shader, vertex_shader);
	glAttachShader(shader, fragment_shader);
	glBindAttribLocation(shader, VATT_POSITION, "i_position");
	glBindAttribLocation(shader, VATT_COLOR, "i_color");
	glBindAttribLocation(shader, VATT_UV, "i_uv");
	glLinkProgram(shader);

	glGetProgramiv(shader, GL_LINK_STATUS, &success);
	if (!success) {
		GLchar error[1024];
		glGetProgramInfoLog(shader, sizeof(error), NULL, error);
		FATAL("Shader fail:\n%s", error);
	}

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	uniforms.mvp = glGetUniformLocation(shader, "u_mvp");
	uniforms.texture = glGetUniformLocation(shader, "u_texture");
	uniforms.alpha_test = glGetUniformLocation(shader, "u_alpha_test");
	uniforms.stencil = glGetUniformLocation(shader, "u_stencil");

	glEnable(GL_BLEND);
	glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
	glUseProgram(shader);
	glActiveTexture(GL_TEXTURE0);
	glUniform1i(uniforms.texture, 0);
	glDepthFunc(GL_LEQUAL);

	textures = NewTinyMap();
}

void video_teardown() {
	glDeleteVertexArrays(1, &batch.vao);
	glDeleteBuffers(1, &batch.vbo);
	SDL_free(batch.vertices);

	glDeleteTextures(1, &blank_texture);
	FreeTinyMap(textures);

	glDeleteProgram(shader);

	SDL_GL_DestroyContext(gpu);
	SDL_DestroyWindow(window);
}

// Start of video loop.
void video_start() {
	glEnable(GL_DEPTH_TEST);
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

// End of video loop.
void video_end() {
	SDL_GL_SwapWindow(window);
}

// ========
// SURFACES
// ========

// Makes a surface.
Surface* create_surface(GLuint width, GLuint height, bool color, bool depth) {
	Surface* surface = SDL_malloc(sizeof(*surface));
	if (surface == NULL)
		FATAL("create_surface fail");
	SDL_memset(surface, 0, sizeof(*surface));

	glm_mat4_identity(surface->model_matrix);
	glm_mat4_identity(surface->view_matrix);
	glm_mat4_identity(surface->projection_matrix);
	glm_mat4_identity(surface->mvp_matrix);

	surface->enabled[SURF_COLOR] = color;
	surface->enabled[SURF_DEPTH] = depth;
	surface->size[0] = SDL_max(width, 1);
	surface->size[1] = SDL_max(height, 1);

	return surface;
}

// Nukes the surface.
void destroy_surface(Surface* surface) {
	dispose_surface(surface);
	SDL_free(surface);
}

// Checks the surface and adjusts its framebuffer.
void check_surface(Surface* surface) {
	if (surface->fbo == 0)
		glGenFramebuffers(1, &surface->fbo);

	if (surface->enabled[SURF_COLOR] && surface->texture[SURF_COLOR] == 0) {
		glGenTextures(1, &surface->texture[SURF_COLOR]);

		glBindFramebuffer(GL_FRAMEBUFFER, surface->fbo);
		glBindTexture(GL_TEXTURE_2D, surface->texture[SURF_COLOR]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)(surface->size[0]), (GLsizei)(surface->size[1]), 0,
			GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

		glFramebufferTexture2D(
			GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, surface->texture[SURF_COLOR], 0);
	} else if (surface->texture[SURF_COLOR] != 0) {
		glDeleteTextures(1, &surface->texture[SURF_COLOR]);
		surface->texture[SURF_COLOR] = 0;
	}

	if (surface->enabled[SURF_DEPTH] && surface->texture[SURF_DEPTH] == 0) {
		glGenTextures(1, &surface->texture[SURF_DEPTH]);

		glBindFramebuffer(GL_FRAMEBUFFER, surface->fbo);
		glBindTexture(GL_TEXTURE_2D, surface->texture[SURF_DEPTH]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, (GLsizei)(surface->size[0]),
			(GLsizei)(surface->size[1]), 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

		glFramebufferTexture2D(
			GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, surface->texture[SURF_DEPTH], 0);
	} else if (surface->texture[SURF_DEPTH] != 0) {
		glDeleteTextures(1, &surface->texture[SURF_DEPTH]);
		surface->texture[SURF_DEPTH] = 0;
	}
}

// Nukes the surface's framebuffer.
void dispose_surface(Surface* surface) {
	if (current_surface == surface)
		pop_surface();
	if (surface->active)
		FATAL("Disposing an active surface?");

	if (surface->fbo != 0) {
		glDeleteFramebuffers(1, &surface->fbo);
		surface->fbo = 0;
	}

	if (surface->texture[SURF_COLOR] != 0) {
		glDeleteTextures(1, &surface->texture[SURF_COLOR]);
		surface->texture[SURF_COLOR] = 0;
	}

	if (surface->texture[SURF_DEPTH] != 0) {
		glDeleteTextures(1, &surface->texture[SURF_DEPTH]);
		surface->texture[SURF_DEPTH] = 0;
	}
}

void push_surface(Surface* surface) {}
void pop_surface() {}
