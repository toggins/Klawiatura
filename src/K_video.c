#include "K_file.h"
#include "K_game.h"
#include "K_log.h"
#include "K_memory.h"
#include "K_string.h"
#include "K_video.h"

#include <SDL3_image/SDL_image.h>

static SDL_Window* window = NULL;
static SDL_GLContext gpu = NULL;

static int screen_width = SCREEN_WIDTH;
static int screen_height = SCREEN_HEIGHT;

static GLuint shader = 0;
static Uniforms uniforms = {-1};

static GLuint blank_texture = 0;
static StTinyMap* textures = NULL;
static StTinyMap* fonts = NULL;

static VertexBatch batch = {0};
static Surface* current_surface = NULL;

static mat4 model_matrix = GLM_MAT4_IDENTITY;
static mat4 view_matrix = GLM_MAT4_IDENTITY;
static mat4 projection_matrix = GLM_MAT4_IDENTITY;
static mat4 mvp_matrix = GLM_MAT4_IDENTITY;

#include "embeds/shaders/fragment.glsl"
#include "embeds/shaders/vertex.glsl"

VideoState video_state = {0};

#define CHECK_GL_EXTENSION(ext)                                                                                        \
	do {                                                                                                           \
		if ((ext))                                                                                             \
			break;                                                                                         \
		FATAL("Missing OpenGL extension: " #ext "\nAt least OpenGL 3.3 with shader support is required.");     \
	} while (0)

void video_init(bool force_shader) {
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	// Window
	window = SDL_CreateWindow("Klawiatura", SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
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

	if (force_shader) {
		INFO("! Bypassing OpenGL support checks");
		goto bypass;
	}

	CHECK_GL_EXTENSION(GLAD_GL_ARB_shader_objects);
	CHECK_GL_EXTENSION(GLAD_GL_ARB_vertex_shader);
	CHECK_GL_EXTENSION(GLAD_GL_ARB_fragment_shader);
	CHECK_GL_EXTENSION(GLAD_GL_ARB_vertex_program);
	CHECK_GL_EXTENSION(GLAD_GL_ARB_fragment_program);

bypass:
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
	batch.tint[0] = batch.tint[1] = batch.tint[2] = batch.tint[3] = 1;
	batch.stencil = 0;
	batch.texture = blank_texture;
	batch.alpha_test = 0.5f;

	batch.blend_src[0] = batch.blend_src[1] = GL_SRC_ALPHA;
	batch.blend_dest[0] = GL_ONE_MINUS_SRC_ALPHA;
	batch.blend_dest[1] = GL_ONE;
	batch.logic = GL_COPY;

	batch.filter = false;

	GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex_shader, 1, &shaders_vertex_glsl, NULL);
	glCompileShader(vertex_shader);

	GLint success;
	glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLchar error[1024];
		glGetShaderInfoLog(vertex_shader, sizeof(error), NULL, error);
		FATAL("Vertex shader fail: %s", error);
	}

	GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment_shader, 1, &shaders_fragment_glsl, NULL);
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
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
	glUseProgram(shader);
	glActiveTexture(GL_TEXTURE0);
	glUniform1i(uniforms.texture, 0);
	glDepthFunc(GL_LEQUAL);

	glm_ortho(0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, -16000, 16000, projection_matrix);
	glm_mat4_mul(view_matrix, model_matrix, mvp_matrix);
	glm_mat4_mul(projection_matrix, mvp_matrix, mvp_matrix);

	textures = NewTinyMap();
	fonts = NewTinyMap();
}

#undef CHECK_GL_EXTENSION

void video_teardown() {
	glDeleteVertexArrays(1, &batch.vao);
	glDeleteBuffers(1, &batch.vbo);
	SDL_free(batch.vertices);

	glDeleteTextures(1, &blank_texture);
	FreeTinyMap(textures);
	FreeTinyMap(fonts);

	glDeleteProgram(shader);

	SDL_GL_DestroyContext(gpu);
	SDL_DestroyWindow(window);
}

void start_drawing() {
	const float scalew = (float)screen_width / (float)SCREEN_WIDTH;
	const float scaleh = (float)screen_height / (float)SCREEN_HEIGHT;
	const float scale = SDL_min(scalew, scaleh);

	const float left = (((float)screen_width - (SCREEN_WIDTH * scale)) / -2.f) / scale;
	const float top = (((float)screen_height - (SCREEN_HEIGHT * scale)) / -2.f) / scale;
	const float right = left + ((float)screen_width / scale);
	const float bottom = top + ((float)screen_height / scale);

	glm_ortho(left, right, bottom, top, -16000, 16000, projection_matrix);
	glm_mat4_mul(view_matrix, model_matrix, mvp_matrix);
	glm_mat4_mul(projection_matrix, mvp_matrix, mvp_matrix);
	glViewport(0, 0, screen_width, screen_height);
}

void stop_drawing() {
	submit_batch();
	SDL_GL_SwapWindow(window);
}

#define FOCUS_IMPOSSIBLE (SDL_WINDOW_OCCLUDED | SDL_WINDOW_MINIMIZED | SDL_WINDOW_NOT_FOCUSABLE)
#define HAS_FOCUS                                                                                                      \
	(SDL_WINDOW_MOUSE_GRABBED | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_CAPTURE | SDL_WINDOW_KEYBOARD_GRABBED)
bool window_focused() {
	const SDL_WindowFlags flags = SDL_GetWindowFlags(window);
	return !(flags & FOCUS_IMPOSSIBLE) && (flags & HAS_FOCUS);
}

bool window_start_typing() {
	return SDL_StartTextInput(window);
}

void window_stop_typing() {
	SDL_StopTextInput(window);
}

// =====
// STATE
// =====

void start_video_state() {}
void nuke_video_state() {}

// =======
// DISPLAY
// =======

void get_resolution(int* width, int* height) {
	SDL_GetWindowSizeInPixels(window, width, height);
}

void set_resolution(int width, int height) {
	SDL_SetWindowSize(window, width <= 0 ? SCREEN_WIDTH : width, height <= 0 ? SCREEN_HEIGHT : height);
	SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
	SDL_SyncWindow(window);
	get_resolution(&screen_width, &screen_height);
}

bool get_fullscreen() {
	return SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN;
}

void set_fullscreen(bool fullscreen) {
	SDL_SetWindowFullscreen(window, fullscreen);
	SDL_SyncWindow(window);
	SDL_GetWindowSizeInPixels(window, &screen_width, &screen_height);
}

bool get_vsync() {
	int interval = 0;
	SDL_GL_GetSwapInterval(&interval);
	return interval != 0;
}

void set_vsync(bool vsync) {
	SDL_GL_SetSwapInterval(vsync);
}

// =====
// BASIC
// =====

/// Clears the current render target's color buffer.
void clear_color(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
	glClearColor(r, g, b, a);
	glClear(GL_COLOR_BUFFER_BIT);
}

/// Clears the current render target's depth buffer.
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

	const char* file = find_data_file(fmt("data/textures/%s.*", name), ".json");
	if (file == NULL) {
		WTF("Texture \"%s\" not found", name);
		return;
	}

	SDL_Surface* surface = IMG_Load(file);
	if (surface == NULL) {
		WTF("Texture \"%s\" fail: %s", name, SDL_GetError());
		return;
	}

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

	file = find_data_file(fmt("data/textures/%s.json", name), NULL);
	if (file == NULL)
		goto eatadick;

	yyjson_doc* json = yyjson_read_file(file, JSON_READ_FLAGS, NULL, NULL);
	if (json == NULL)
		goto eatadick;

	yyjson_val* root = yyjson_doc_get_root(json);
	if (!yyjson_is_obj(root))
		goto eattwodicks;

	texture.offset[0] = (GLfloat)yyjson_get_num(yyjson_obj_get(root, "x_offset"));
	texture.offset[1] = (GLfloat)yyjson_get_num(yyjson_obj_get(root, "y_offset"));

eattwodicks:
	yyjson_doc_free(json);
eatadick:
	StMapPut(textures, long_key(name), &texture, sizeof(texture))->cleanup = nuke_texture;
}

const Texture* get_texture(const char* name) {
	return (name == NULL) ? NULL : StMapGet(textures, long_key(name));
}

// =====
// FONTS
// =====

static void nuke_font(void* ptr) {
	Font* font = ptr;
	SDL_free(font->name);
}

void load_font(const char* name) {
	if (name == NULL || get_font(name) != NULL)
		return;

	Font font = {0};

	const char* file = find_data_file(fmt("data/fonts/%s.*", name), NULL);
	if (file == NULL) {
		WTF("Font \"%s\" not found", name);
		return;
	}

	yyjson_read_err error;
	yyjson_doc* json = yyjson_read_file(file, JSON_READ_FLAGS, NULL, &error);
	if (json == NULL) {
		WTF("Font \"%s\" fail: %s", name, error.msg);
		return;
	}

	yyjson_val* root = yyjson_doc_get_root(json);
	if (!yyjson_is_obj(root)) {
		WTF("Font \"%s\" fail, expected object got %s", name, yyjson_get_type_desc(root));
		yyjson_doc_free(json);
		return;
	}

	const char* tname = yyjson_get_str(yyjson_obj_get(root, "texture"));
	load_texture(tname);
	font.texture = get_texture(tname);
	if (font.texture == NULL) {
		WTF("Font \"%s\" has invalid texture \"%s\"", name, tname);
		yyjson_doc_free(json);
		return;
	}

	font.name = SDL_strdup(name);
	font.height = (GLfloat)yyjson_get_num(yyjson_obj_get(root, "height"));
	font.spacing = (GLfloat)yyjson_get_num(yyjson_obj_get(root, "spacing"));

	yyjson_val* glyphmap = yyjson_obj_get(root, "glyphs");
	if (!yyjson_is_obj(glyphmap))
		goto eatadick;

	const GLfloat width = (GLfloat)font.texture->size[0];
	const GLfloat height = (GLfloat)font.texture->size[1];

	size_t i, n;
	yyjson_val *key, *value;
	yyjson_obj_foreach(glyphmap, i, n, key, value) {
		const char* kname = yyjson_get_str(key);
		if (kname == NULL)
			continue;

		char gid = kname[0];
		if (gid < 0 || gid > CHAR_MAX) {
			WTF("Unsupported or invalid glyph \"%c\" in font \"%s\"", gid, name);
			continue;
		}

		if (!yyjson_is_obj(value)) {
			WTF("Expected font \"%s\" glyph \"%c\" as object, got %s", name, gid,
				yyjson_get_type_desc(value));
			continue;
		}

		Glyph* glyph = &font.glyphs[gid];
		glyph->width = (GLfloat)yyjson_get_num(yyjson_obj_get(value, "width"));

		yyjson_val* uvs = yyjson_obj_get(value, "uvs");
		if (uvs == NULL)
			continue;
		if (!yyjson_is_arr(uvs)) {
			WTF("Expected font \"%s\" glyph \"%c\" UVs as array, got %s", name, gid,
				yyjson_get_type_desc(value));
			continue;
		}
		glyph->uvs[0] = (GLfloat)yyjson_get_num(yyjson_arr_get(uvs, 0)) / width;
		glyph->uvs[1] = (GLfloat)yyjson_get_num(yyjson_arr_get(uvs, 1)) / height;
		glyph->uvs[2] = (GLfloat)yyjson_get_num(yyjson_arr_get(uvs, 2)) / width;
		glyph->uvs[3] = (GLfloat)yyjson_get_num(yyjson_arr_get(uvs, 3)) / height;
	}

eatadick:
	yyjson_doc_free(json);

	StMapPut(fonts, long_key(name), &font, sizeof(font))->cleanup = nuke_texture;
}

const Font* get_font(const char* name) {
	return (name == NULL) ? NULL : StMapGet(fonts, long_key(name));
}

// =====
// BATCH
// =====

void batch_alpha_test(GLfloat threshold) {
	if (batch.alpha_test != threshold) {
		submit_batch();
		batch.alpha_test = threshold;
	}
}

static void batch_texture(GLuint tex) {
	if (batch.texture != tex) {
		submit_batch();
		batch.texture = tex;
	}
}

static void batch_vertex(const GLfloat pos[3], const GLubyte color[4], const GLfloat uv[2]) {
	if (batch.vertex_count < batch.vertex_capacity)
		goto just_push;

	submit_batch();

	const size_t new_size = batch.vertex_capacity * 2;
	if (new_size < batch.vertex_capacity)
		FATAL("Capacity overflow in vertex batch");
	batch.vertices = SDL_realloc(batch.vertices, new_size * sizeof(Vertex));
	if (batch.vertices == NULL)
		FATAL("Out of memory for vertex batch");

	batch.vertex_capacity = new_size;

	glBindBuffer(GL_ARRAY_BUFFER, batch.vbo);
	glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(Vertex) * batch.vertex_capacity), NULL, GL_DYNAMIC_DRAW);

just_push:
#define CLR(idx) (GLubyte)(batch.tint[idx] * (GLfloat)color[idx])
	batch.vertices[batch.vertex_count++] = (Vertex){
		.position = {pos[0], pos[1], pos[2]},
		.color = {CLR(0), CLR(1), CLR(2), CLR(3)},
		.uv = {uv[0], uv[1]},
	};
#undef CLR
}

void batch_cursor(const GLfloat x, const GLfloat y, const GLfloat z) {
	batch.cursor[0] = x;
	batch.cursor[1] = y;
	batch.cursor[2] = z;
}

void batch_color(const GLubyte color[4]) {
	SDL_memcpy(batch.color, color, sizeof(batch.color));
}

void batch_angle(const GLfloat angle) {
	batch.angle = angle;
}

/// Adds a texture as a sprite to the vertex batch.
void batch_sprite(const char* name, const GLboolean flip[2]) {
	const Texture* texture = get_texture(name);
	if (texture == NULL)
		return;

	batch_texture(texture->texture);

	// Position
	const GLfloat x1 = -(flip[0] ? ((GLfloat)(texture->size[0]) - (texture->offset[0])) : (texture->offset[0]));
	const GLfloat y1 = -(flip[1] ? ((GLfloat)(texture->size[1]) - (texture->offset[1])) : (texture->offset[1]));
	const GLfloat x2 = x1 + (GLfloat)texture->size[0];
	const GLfloat y2 = y1 + (GLfloat)texture->size[1];
	const GLfloat z = batch.cursor[2];

	vec2 p1 = {x1, y1};
	vec2 p2 = {x2, y1};
	vec2 p3 = {x1, y2};
	vec2 p4 = {x2, y2};
	if (batch.angle != 0) {
		glm_vec2_rotate(p1, batch.angle, p1);
		glm_vec2_rotate(p2, batch.angle, p2);
		glm_vec2_rotate(p3, batch.angle, p3);
		glm_vec2_rotate(p4, batch.angle, p4);
	}
	glm_vec2_add((GLfloat*)batch.cursor, p1, p1);
	glm_vec2_add((GLfloat*)batch.cursor, p2, p2);
	glm_vec2_add((GLfloat*)batch.cursor, p3, p3);
	glm_vec2_add((GLfloat*)batch.cursor, p4, p4);

	// UVs
	const GLfloat u1 = flip[0];
	const GLfloat v1 = flip[1];
	const GLfloat u2 = (GLfloat)(!flip[0]);
	const GLfloat v2 = (GLfloat)(!flip[1]);

	// Vertices
	batch_vertex(XYZ(p3[0], p3[1], z), batch.color, UV(u1, v2));
	batch_vertex(XYZ(p1[0], p1[1], z), batch.color, UV(u1, v1));
	batch_vertex(XYZ(p2[0], p2[1], z), batch.color, UV(u2, v1));
	batch_vertex(XYZ(p2[0], p2[1], z), batch.color, UV(u2, v1));
	batch_vertex(XYZ(p4[0], p4[1], z), batch.color, UV(u2, v2));
	batch_vertex(XYZ(p3[0], p3[1], z), batch.color, UV(u1, v2));
}

/// Adds a surface to the vertex batch.
void batch_surface(Surface* surface) {
	if (surface == NULL)
		return;
	if (surface->active)
		FATAL("Drawing an active surface?");
	if (surface->texture[SURF_COLOR] == 0)
		return;

	batch_texture(surface->texture[SURF_COLOR]);

	// Position
	const GLfloat x1 = batch.cursor[0];
	const GLfloat y1 = batch.cursor[1];
	const GLfloat x2 = x1 + (GLfloat)surface->size[0];
	const GLfloat y2 = y1 + (GLfloat)surface->size[1];
	const GLfloat z = batch.cursor[2];

	// Vertices
	batch_vertex(XYZ(x1, y2, z), batch.color, UV(0, 1));
	batch_vertex(XYZ(x1, y1, z), batch.color, UV(0, 0));
	batch_vertex(XYZ(x2, y1, z), batch.color, UV(1, 0));
	batch_vertex(XYZ(x2, y1, z), batch.color, UV(1, 0));
	batch_vertex(XYZ(x2, y2, z), batch.color, UV(1, 1));
	batch_vertex(XYZ(x1, y2, z), batch.color, UV(0, 1));
}

static GLfloat string_width(const Font* font, GLfloat size, const char* str) {
	if (font == NULL)
		return 0;

	GLfloat width = 0;
	GLfloat cx = 0;
	const GLfloat scale = size / font->height;
	const GLfloat spacing = font->spacing * scale;

	size_t bytes = SDL_strlen(str);
	while (bytes > 0) {
		uint32_t gid = SDL_StepUTF8(&str, &bytes);

		// Special/invalid characters
		if (gid == '\r')
			continue;
		if (gid == '\n') {
			cx = 0;
			continue;
		}
		if (SDL_isspace((int)gid))
			gid = ' ';
		else if (gid <= 0 || gid > CHAR_MAX)
			gid = '?';

		// Valid glyph
		cx += font->glyphs[gid].width * scale;
		if (bytes > 0)
			cx += spacing;

		width = SDL_max(width, cx);
	}

	return width;
}

static GLfloat string_height(const Font* font, GLfloat size, const char* str) {
	if (font == NULL)
		return 0;

	size_t bytes = SDL_strlen(str);
	GLfloat height = (bytes > 0) ? font->height : 0;
	while (bytes > 0)
		if (SDL_StepUTF8(&str, &bytes) == '\n')
			height += font->height;

	return height;
}

/// Adds a string to the vertex batch.
void batch_string(const char* font_name, GLfloat size, const FontAlignment alignment[2], const char* str) {
	const Font* font = get_font(font_name);
	if (font == NULL || str == NULL)
		return;

	batch_texture(font->texture->texture);

	// Origin
	GLfloat ox = batch.cursor[0];
	GLfloat oy = batch.cursor[1];

	// Horizontal alignment
	switch (alignment[0]) {
	case FA_CENTER:
		ox -= string_width(font, size, str) / 2.f;
		break;
	case FA_RIGHT:
		ox -= string_width(font, size, str);
		break;
	default:
		break;
	}

	// Vertical alignment
	switch (alignment[1]) {
	case FA_MIDDLE:
		oy -= string_height(font, size, str) / 2.f;
		break;
	case FA_BOTTOM:
		oy -= string_height(font, size, str);
		break;
	default:
		break;
	}

	GLfloat cx = ox, cy = oy;
	const GLfloat scale = size / font->height;
	const GLfloat spacing = font->spacing * scale;

	size_t bytes = SDL_strlen(str);
	while (bytes > 0) {
		uint32_t gid = SDL_StepUTF8(&str, &bytes);

		// Special/invalid characters
		if (gid == '\r')
			continue;
		if (gid == '\n') {
			cx = ox;
			cy += size;
			continue;
		}
		if (SDL_isspace((int)gid))
			gid = ' ';
		else if (gid < 0 || gid > CHAR_MAX)
			gid = '?';

		// Valid glyph
		const Glyph* glyph = &font->glyphs[gid];
		const GLfloat width = glyph->width * scale;

		const GLfloat x1 = cx;
		const GLfloat y1 = cy;
		const GLfloat x2 = x1 + width;
		const GLfloat y2 = y1 + size;
		batch_vertex(XYZ(x1, y2, batch.cursor[2]), batch.color, UV(glyph->uvs[0], glyph->uvs[3]));
		batch_vertex(XYZ(x1, y1, batch.cursor[2]), batch.color, UV(glyph->uvs[0], glyph->uvs[1]));
		batch_vertex(XYZ(x2, y1, batch.cursor[2]), batch.color, UV(glyph->uvs[2], glyph->uvs[1]));
		batch_vertex(XYZ(x2, y1, batch.cursor[2]), batch.color, UV(glyph->uvs[2], glyph->uvs[1]));
		batch_vertex(XYZ(x2, y2, batch.cursor[2]), batch.color, UV(glyph->uvs[2], glyph->uvs[3]));
		batch_vertex(XYZ(x1, y2, batch.cursor[2]), batch.color, UV(glyph->uvs[0], glyph->uvs[3]));

		cx += width;
		if (bytes > 0)
			cx += spacing;
	}
}

/// Dumps the vertex batch on your screen.
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
	submit_batch();
	glm_mat4_copy(matrix, (current_surface == NULL) ? model_matrix : current_surface->model_matrix);
}

void set_view_matrix(mat4 matrix) {
	submit_batch();
	glm_mat4_copy(matrix, (current_surface == NULL) ? view_matrix : current_surface->view_matrix);
}

void set_projection_matrix(mat4 matrix) {
	submit_batch();
	glm_mat4_copy(matrix, (current_surface == NULL) ? projection_matrix : current_surface->projection_matrix);
}

void apply_matrices() {
	submit_batch();
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

/// Nukes the surface.
void destroy_surface(Surface* surface) {
	dispose_surface(surface);
	SDL_free(surface);
}

static void make_surface_buffer(Surface* surface, size_t idx) {
	glGenTextures(1, &surface->texture[idx]);

	glBindFramebuffer(GL_FRAMEBUFFER, surface->fbo);
	glBindTexture(GL_TEXTURE_2D, surface->texture[idx]);

	const GLsizei width = (GLsizei)surface->size[0];
	const GLsizei height = (GLsizei)surface->size[1];
	if (idx == SURF_COLOR)
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	else
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, width, height, 0, GL_DEPTH_STENCIL,
			GL_UNSIGNED_INT_24_8, NULL);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

	const GLenum attachment = idx == SURF_COLOR ? GL_COLOR_ATTACHMENT0 : GL_DEPTH_STENCIL_ATTACHMENT;
	glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, surface->texture[idx], 0);
}

static void check_surface_buffer(Surface* surface, size_t idx) {
	if (surface->enabled[idx]) {
		if (!surface->texture[idx])
			make_surface_buffer(surface, idx);
	} else if (surface->texture[idx] != 0) {
		glDeleteTextures(1, &surface->texture[idx]);
		surface->texture[idx] = 0;
	}
}

/// Checks the surface for a valid framebuffer.
void check_surface(Surface* surface) {
	if (surface->fbo == 0)
		glGenFramebuffers(1, &surface->fbo);
	check_surface_buffer(surface, SURF_COLOR);
	check_surface_buffer(surface, SURF_DEPTH);
}

/// Nukes the surface's framebuffer.
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

void resize_surface(Surface* surface, GLuint width, GLuint height) {
	if (surface->active)
		FATAL("Resizing an active surface?");
	if (surface->size[0] == width && surface->size[1] == height)
		return;

	dispose_surface(surface);
	surface->size[0] = width;
	surface->size[1] = height;
}

/// Pushes an INACTIVE surface onto the stack.
void push_surface(Surface* surface) {
	if (surface == NULL)
		FATAL("Pushing a null surface?");
	if (current_surface == surface)
		FATAL("Pushing the current surface?");

	submit_batch();

	if (surface == NULL) {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
		glDisable(GL_DEPTH_TEST);
	} else {
		if (surface->active)
			FATAL("Pushing an active surface?");
		surface->active = true;
		surface->previous = current_surface;

		check_surface(surface);
		glBindFramebuffer(GL_FRAMEBUFFER, surface->fbo);
		glViewport(0, 0, (GLsizei)(surface->size[0]), (GLsizei)(surface->size[1]));
		(surface->enabled[SURF_DEPTH] ? glEnable : glDisable)(GL_DEPTH_TEST);
	}

	current_surface = surface;
}

/// Pops an ACTIVE surface off the stack.
void pop_surface() {
	if (current_surface == NULL)
		FATAL("Popping a null surface?");

	submit_batch();

	Surface* surface = current_surface->previous;
	current_surface->active = false;
	current_surface->previous = NULL;

	if (surface == NULL) {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, screen_width, screen_height);
		glDisable(GL_DEPTH_TEST);
	} else {
		check_surface(surface);
		glBindFramebuffer(GL_FRAMEBUFFER, surface->fbo);
		glViewport(0, 0, (GLsizei)(surface->size[0]), (GLsizei)(surface->size[1]));
		(surface->enabled[SURF_DEPTH] ? glEnable : glDisable)(GL_DEPTH_TEST);
	}

	current_surface = surface;
}
