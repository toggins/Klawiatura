#include <SDL3_image/SDL_image.h>

#include "K_file.h"
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

static mat4 model_matrix = GLM_MAT4_IDENTITY;
static mat4 view_matrix = GLM_MAT4_IDENTITY;
static mat4 projection_matrix = GLM_MAT4_IDENTITY;
static mat4 mvp_matrix = GLM_MAT4_IDENTITY;

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

	glEnable(GL_BLEND | GL_CULL_FACE);
	glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
	glUseProgram(shader);
	glActiveTexture(GL_TEXTURE0);
	glUniform1i(uniforms.texture, 0);
	glDepthFunc(GL_LEQUAL);
	glCullFace(GL_BACK);

	glm_ortho(0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, -16000, 16000, projection_matrix);
	glm_mat4_mul(view_matrix, model_matrix, mvp_matrix);
	glm_mat4_mul(projection_matrix, mvp_matrix, mvp_matrix);

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

// End of rendering step.
void submit_video() {
	submit_batch();
	SDL_GL_SwapWindow(window);
}

// =====
// BASIC
// =====

void clear_color(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
	glClearColor(r, g, b, a);
	glClear(GL_COLOR_BUFFER_BIT);
}

void clear_depth(GLfloat depth) {
	glClearDepthf(depth);
	glClear(GL_DEPTH_BUFFER_BIT);
}

// ========
// TEXTURES
// ========

static void nuke_texture(void* ptr) {
	Texture* texture = ptr;
	SDL_free(texture->name);
	glDeleteTextures(1, &texture->texture);
}

void load_texture(const char* name) {
	if (name == NULL || get_texture(name) != NULL)
		return;

	Texture texture = {0};

	const char* file = find_file(file_pattern("data/textures/%s.*", name), ".json");
	if (file == NULL)
		FATAL("Texture \"%s\" not found", name);
	SDL_Surface* surface = IMG_Load(file);
	if (surface == NULL)
		FATAL("Texture \"%s\" fail: %s", name, SDL_GetError());

	texture.name = SDL_strdup(name);
	texture.size[0] = surface->w;
	texture.size[1] = surface->h;

	glGenTextures(1, &(texture.texture));
	glBindTexture(GL_TEXTURE_2D, texture.texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

	SDL_Surface* temp = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
	if (temp == NULL)
		FATAL("Texture \"%s\" conversion fail: %s", name, SDL_GetError());
	SDL_DestroySurface(surface);
	surface = temp;

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, surface->w, surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);
	SDL_DestroySurface(surface);

	file = find_file(file_pattern("data/textures/%s.json", name), NULL);
	if (file != NULL) {
		yyjson_doc* json = yyjson_read_file(
			file, YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS, NULL, NULL);
		if (json != NULL) {
			yyjson_val* root = yyjson_doc_get_root(json);
			if (yyjson_is_obj(root)) {
				texture.offset[0] = (GLfloat)yyjson_get_num(yyjson_obj_get(root, "x_offset"));
				texture.offset[1] = (GLfloat)yyjson_get_num(yyjson_obj_get(root, "y_offset"));
			}
			yyjson_doc_free(json);
		}
	}

	StMapPut(textures, long_key(name), &texture, sizeof(texture))->cleanup = nuke_texture;
}

const Texture* get_texture(const char* name) {
	return (name == NULL) ? NULL : StMapGet(textures, long_key(name));
}

// =====
// BATCH
// =====

void set_batch_texture(GLuint tex) {
	if (batch.texture != tex) {
		submit_batch();
		batch.texture = tex;
	}
}

void batch_vertex(const GLfloat pos[3], const GLubyte color[4], const GLfloat uv[2]) {
	if (batch.vertex_count >= batch.vertex_capacity) {
		submit_batch();

		const size_t new_size = batch.vertex_capacity * 2;
		if (new_size < batch.vertex_capacity)
			FATAL("Capacity overflow in vertex batch");
		batch.vertices = SDL_realloc(batch.vertices, new_size * sizeof(Vertex));
		if (batch.vertices == NULL)
			FATAL("Out of memory for vertex batch");

		batch.vertex_capacity = new_size;

		glBindBuffer(GL_ARRAY_BUFFER, batch.vbo);
		glBufferData(
			GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(Vertex) * batch.vertex_capacity), NULL, GL_DYNAMIC_DRAW);
	}

	batch.vertices[batch.vertex_count++]
		= (Vertex){pos[0], pos[1], pos[2], (GLubyte)(batch.color[0] * (GLfloat)color[0]),
			(GLubyte)(batch.color[1] * (GLfloat)color[1]), (GLubyte)(batch.color[2] * (GLfloat)color[2]),
			(GLubyte)(batch.color[3] * (GLfloat)color[2]), uv[0], uv[1]};
}

void batch_sprite(
	const char* name, const GLfloat pos[3], const GLboolean flip[2], GLfloat angle, const GLubyte color[4]) {
	const Texture* texture = get_texture(name);
	if (texture == NULL)
		return;

	set_batch_texture(texture->texture);

	// Position
	const GLfloat x1 = -(flip[0] ? ((GLfloat)(texture->size[0]) - (texture->offset[0])) : (texture->offset[0]));
	const GLfloat y1 = -(flip[1] ? ((GLfloat)(texture->size[1]) - (texture->offset[1])) : (texture->offset[1]));
	const GLfloat x2 = x1 + (GLfloat)texture->size[0];
	const GLfloat y2 = y1 + (GLfloat)texture->size[1];
	const GLfloat z = pos[2];

	vec2 p1 = {x1, y1};
	vec2 p2 = {x2, y1};
	vec2 p3 = {x1, y2};
	vec2 p4 = {x2, y2};
	if (angle != 0) {
		glm_vec2_rotate(p1, angle, p1);
		glm_vec2_rotate(p2, angle, p2);
		glm_vec2_rotate(p3, angle, p3);
		glm_vec2_rotate(p4, angle, p4);
	}
	glm_vec2_add((GLfloat*)pos, p1, p1);
	glm_vec2_add((GLfloat*)pos, p2, p2);
	glm_vec2_add((GLfloat*)pos, p3, p3);
	glm_vec2_add((GLfloat*)pos, p4, p4);

	// UVs
	const GLfloat u1 = flip[0];
	const GLfloat v1 = flip[1];
	const GLfloat u2 = (GLfloat)(!flip[0]);
	const GLfloat v2 = (GLfloat)(!flip[1]);

	// Vertices
	batch_vertex(XYZ(p3[0], p3[1], z), color, UV(u1, v2));
	batch_vertex(XYZ(p1[0], p1[1], z), color, UV(u1, v1));
	batch_vertex(XYZ(p2[0], p2[1], z), color, UV(u2, v1));
	batch_vertex(XYZ(p2[0], p2[1], z), color, UV(u2, v1));
	batch_vertex(XYZ(p4[0], p4[1], z), color, UV(u2, v2));
	batch_vertex(XYZ(p3[0], p3[1], z), color, UV(u1, v2));
}

void batch_surface(Surface* surface, const GLfloat pos[3], const GLubyte color[4]) {
	if (surface == NULL)
		return;
	if (surface->active)
		FATAL("Drawing an active surface?");
	if (surface->texture[SURF_COLOR] == 0)
		return;

	set_batch_texture(surface->texture[SURF_COLOR]);

	// Position
	const GLfloat x1 = pos[0];
	const GLfloat y1 = pos[1];
	const GLfloat x2 = x1 + (GLfloat)surface->size[0];
	const GLfloat y2 = y1 + (GLfloat)surface->size[1];
	const GLfloat z = pos[2];

	// Vertices
	batch_vertex(XYZ(x1, y2, z), color, UV(0, 1));
	batch_vertex(XYZ(x1, y1, z), color, UV(0, 0));
	batch_vertex(XYZ(x2, y1, z), color, UV(1, 0));
	batch_vertex(XYZ(x2, y1, z), color, UV(1, 0));
	batch_vertex(XYZ(x2, y2, z), color, UV(1, 1));
	batch_vertex(XYZ(x1, y2, z), color, UV(0, 1));
}

void submit_batch() {
	if (batch.vertex_count <= 0)
		return;

	glBindVertexArray(batch.vao);
	glBindBuffer(GL_ARRAY_BUFFER, batch.vbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(sizeof(Vertex) * batch.vertex_count), batch.vertices);

	// Apply texture
	glBindTexture(GL_TEXTURE_2D, batch.texture);
	const GLint filter = batch.filter ? GL_LINEAR : GL_NEAREST;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
	glUniform1f(uniforms.alpha_test, batch.alpha_test);
	glUniform1f(uniforms.stencil, batch.stencil);
	glUniformMatrix4fv(uniforms.mvp, 1, GL_FALSE, (const GLfloat*)(*get_mvp_matrix()));

	// Apply blend mode
	glBlendFuncSeparate(batch.blend_src[0], batch.blend_dest[0], batch.blend_src[1], batch.blend_dest[1]);
	glLogicOp(batch.logic);

	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)batch.vertex_count);
	batch.vertex_count = 0;
}

// ========
// MATRICES
// ========

const mat4* get_mvp_matrix() {
	return (current_surface == NULL) ? &mvp_matrix : &current_surface->mvp_matrix;
}

void set_model_matrix(mat4 matrix) {
	glm_mat4_copy(matrix, (current_surface == NULL) ? model_matrix : current_surface->model_matrix);
}

void set_view_matrix(mat4 matrix) {
	glm_mat4_copy(matrix, (current_surface == NULL) ? view_matrix : current_surface->view_matrix);
}

void set_projection_matrix(mat4 matrix) {
	glm_mat4_copy(matrix, (current_surface == NULL) ? projection_matrix : current_surface->projection_matrix);
}

void apply_matrices() {
	if (current_surface != NULL) {
		glm_mat4_mul(current_surface->view_matrix, current_surface->model_matrix, current_surface->mvp_matrix);
		glm_mat4_mul(
			current_surface->projection_matrix, current_surface->mvp_matrix, current_surface->mvp_matrix);
	} else {
		glm_mat4_mul(view_matrix, model_matrix, mvp_matrix);
		glm_mat4_mul(projection_matrix, mvp_matrix, mvp_matrix);
	}
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
	glm_ortho(0, (float)width, 0, (float)height, -16000, 16000, surface->projection_matrix);
	glm_mat4_mul(surface->view_matrix, surface->model_matrix, surface->mvp_matrix);
	glm_mat4_mul(surface->projection_matrix, surface->mvp_matrix, surface->mvp_matrix);

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

	if (surface->enabled[SURF_COLOR]) {
		if (surface->texture[SURF_COLOR] == 0) {
			glGenTextures(1, &surface->texture[SURF_COLOR]);

			glBindFramebuffer(GL_FRAMEBUFFER, surface->fbo);
			glBindTexture(GL_TEXTURE_2D, surface->texture[SURF_COLOR]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)(surface->size[0]),
				(GLsizei)(surface->size[1]), 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

			glFramebufferTexture2D(
				GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, surface->texture[SURF_COLOR], 0);
		}
	} else if (surface->texture[SURF_COLOR] != 0) {
		glDeleteTextures(1, &surface->texture[SURF_COLOR]);
		surface->texture[SURF_COLOR] = 0;
	}

	if (surface->enabled[SURF_DEPTH]) {
		if (surface->texture[SURF_DEPTH] == 0) {
			glGenTextures(1, &surface->texture[SURF_DEPTH]);

			glBindFramebuffer(GL_FRAMEBUFFER, surface->fbo);
			glBindTexture(GL_TEXTURE_2D, surface->texture[SURF_DEPTH]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, (GLsizei)(surface->size[0]),
				(GLsizei)(surface->size[1]), 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
				surface->texture[SURF_DEPTH], 0);
		}
	} else if (surface->texture[SURF_DEPTH] != 0) {
		glDeleteTextures(1, &surface->texture[SURF_DEPTH]);
		surface->texture[SURF_DEPTH] = 0;
	}
}

// Nukes the surface's framebuffer.
void dispose_surface(Surface* surface) {
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

// Resizes a surface.
void resize_surface(Surface* surface, GLuint width, GLuint height) {
	if (surface->active)
		FATAL("Resizing an active surface?");
	if (surface->size[0] == width && surface->size[1] == height)
		return;

	dispose_surface(surface);
	surface->size[0] = width;
	surface->size[1] = height;
}

// Pushes an INACTIVE surface onto the stack.
void push_surface(Surface* surface) {
	if (surface == NULL)
		FATAL("Pushing a null surface?");
	if (current_surface == surface)
		FATAL("Pushing the current surface?");

	submit_batch();

	if (surface != NULL) {
		if (surface->active)
			FATAL("Pushing an active surface?");
		surface->active = true;
		surface->previous = current_surface;

		check_surface(surface);
		glBindFramebuffer(GL_FRAMEBUFFER, surface->fbo);
		glViewport(0, 0, (GLsizei)(surface->size[0]), (GLsizei)(surface->size[1]));

		if (surface->enabled[SURF_DEPTH])
			glEnable(GL_DEPTH_TEST);
		else
			glDisable(GL_DEPTH_TEST);
	} else {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
		glDisable(GL_DEPTH_TEST);
	}

	current_surface = surface;
}

// Pops an ACTIVE surface off the stack.
void pop_surface() {
	if (current_surface == NULL)
		FATAL("Popping a null surface?");

	submit_batch();

	Surface* surface = current_surface->previous;
	current_surface->active = false;
	current_surface->previous = NULL;

	if (surface != NULL) {
		check_surface(surface);
		glBindFramebuffer(GL_FRAMEBUFFER, surface->fbo);
		glViewport(0, 0, (GLsizei)(surface->size[0]), (GLsizei)(surface->size[1]));

		if (surface->enabled[SURF_DEPTH])
			glEnable(GL_DEPTH_TEST);
		else
			glDisable(GL_DEPTH_TEST);
	} else {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
		glDisable(GL_DEPTH_TEST);
	}

	current_surface = surface;
}
