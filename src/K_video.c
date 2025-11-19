#include <SDL3_image/SDL_image.h>

#include "K_cmd.h"
#include "K_file.h"
#include "K_game.h"
#include "K_log.h"
#include "K_memory.h" // IWYU pragma: keep
#include "K_string.h"
#include "K_video.h"

SDL_Window* window = NULL;
static SDL_GLContext gpu = NULL;

static int window_width = SCREEN_WIDTH, window_height = SCREEN_HEIGHT;

static GLuint shader = 0;
static Uniforms uniforms = {-1};

static GLuint blank_texture = 0;
static StTinyMap *textures = NULL, *fonts = NULL;

static VertexBatch batch = {0};
static Surface* current_surface = NULL;

static mat4 model_matrix = GLM_MAT4_IDENTITY, view_matrix = GLM_MAT4_IDENTITY, projection_matrix = GLM_MAT4_IDENTITY,
	    mvp_matrix = GLM_MAT4_IDENTITY;

static StTinyMap* tilemaps = NULL;
static TileMap* last_tilemap = NULL;

#include "embeds/shaders/fragment.glsl"
#include "embeds/shaders/vertex.glsl"

VideoState video_state = {0};

extern ClientInfo CLIENT;

#define CHECK_GL_EXTENSION(ext)                                                                                        \
	EXPECT((ext), "Missing OpenGL extension: " #ext "\nAt least OpenGL 3.3 with shader support is required.");

void video_init(bool force_shader) {
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	// Window
	window = SDL_CreateWindow("Klawiatura", SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	EXPECT(window, "Window fail: %s", SDL_GetError());

	// OpenGL
	gpu = SDL_GL_CreateContext(window);
	EXPECT(gpu && SDL_GL_MakeCurrent(window, gpu), "GPU fail: %s", SDL_GetError());

	SDL_GL_SetSwapInterval(0);

	int version = gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress);
	EXPECT(version, "Failed to load OpenGL functions");

	INFO("GLAD version: %d.%d", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));
	EXPECT(GLAD_GL_VERSION_3_3,
		"Unsupported OpenGL version\nAt least OpenGL 3.3 with framebuffer and shader support is "
		"required.");

	if (force_shader) {
		WARN("Bypassing OpenGL support checks");
		goto bypass;
	}

	CHECK_GL_EXTENSION(GLAD_GL_ARB_vertex_buffer_object);
	CHECK_GL_EXTENSION(GLAD_GL_ARB_vertex_array_object);
	CHECK_GL_EXTENSION(GLAD_GL_ARB_framebuffer_object);
	CHECK_GL_EXTENSION(GLAD_GL_ARB_shader_objects);
	CHECK_GL_EXTENSION(GLAD_GL_ARB_vertex_shader);
	CHECK_GL_EXTENSION(GLAD_GL_ARB_fragment_shader);

bypass:
	INFO("OpenGL vendor: %s", glGetString(GL_VENDOR));
	INFO("OpenGL version: %s", glGetString(GL_VERSION));
	INFO("OpenGL renderer: %s", glGetString(GL_RENDERER));
	INFO("OpenGL shading language version: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));

	// Blank texture
	glGenTextures(1, &blank_texture);
	glBindTexture(GL_TEXTURE_2D, blank_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, B_WHITE);

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
	EXPECT(batch.vertices, "batch.vertices fail");

	glGenBuffers(1, &batch.vbo);
	glBindBuffer(GL_ARRAY_BUFFER, batch.vbo);
	glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(Vertex) * batch.vertex_capacity), NULL, GL_DYNAMIC_DRAW);

	glEnableVertexAttribArray(VATT_POSITION);
	glVertexAttribPointer(VATT_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
	glEnableVertexAttribArray(VATT_COLOR);
	glVertexAttribPointer(VATT_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex, color));
	glEnableVertexAttribArray(VATT_UV);
	glVertexAttribPointer(VATT_UV, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));

	GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex_shader, 1, &vertex_glsl, NULL);
	glCompileShader(vertex_shader);

	GLint success = 0;
	glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLchar error[1024];
		glGetShaderInfoLog(vertex_shader, sizeof(error), NULL, error);
		FATAL("Vertex shader fail: %s", error);
	}

	GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment_shader, 1, &fragment_glsl, NULL);
	glCompileShader(fragment_shader);

	glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLchar error[1024];
		glGetShaderInfoLog(fragment_shader, sizeof(error), NULL, error);
		FATAL("Fragment shader fail: %s", error);
	}

	shader = glCreateProgram();
	glAttachShader(shader, vertex_shader), glAttachShader(shader, fragment_shader);
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

	glDeleteShader(vertex_shader), glDeleteShader(fragment_shader);

	uniforms.mvp = glGetUniformLocation(shader, "u_mvp");
	uniforms.texture = glGetUniformLocation(shader, "u_texture");
	uniforms.alpha_test = glGetUniformLocation(shader, "u_alpha_test");
	uniforms.stencil = glGetUniformLocation(shader, "u_stencil");

	glEnable(GL_BLEND);
	glEnable(GL_CULL_FACE), glCullFace(GL_FRONT);
	glUseProgram(shader);
	glActiveTexture(GL_TEXTURE0);
	glUniform1i(uniforms.texture, 0);
	glDepthFunc(GL_LEQUAL);

	glm_ortho(0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, -16000, 16000, projection_matrix);
	glm_mat4_mul(view_matrix, model_matrix, mvp_matrix);
	glm_mat4_mul(projection_matrix, mvp_matrix, mvp_matrix);
	glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

	textures = NewTinyMap(), fonts = NewTinyMap();
	batch_reset_hard();
}

#undef CHECK_GL_EXTENSION

void video_teardown() {
	glDeleteVertexArrays(1, &batch.vao);
	glDeleteBuffers(1, &batch.vbo);
	SDL_free(batch.vertices);

	glDeleteTextures(1, &blank_texture);
	FreeTinyMap(textures), FreeTinyMap(fonts);
	FreeTinyMap(tilemaps);

	glDeleteProgram(shader);

	SDL_GL_DestroyContext(gpu);
	SDL_DestroyWindow(window);
}

void start_drawing() {
	const float ww = (float)window_width, wh = (float)window_height;

	const float scalew = ww / (float)SCREEN_WIDTH, scaleh = wh / (float)SCREEN_HEIGHT;
	const float scale = SDL_min(scalew, scaleh);

	const float left = ((ww - ((float)SCREEN_WIDTH * scale)) / -2.f) / scale,
		    top = ((wh - ((float)SCREEN_HEIGHT * scale)) / -2.f) / scale;
	const float right = left + (ww / scale), bottom = top + (wh / scale);

	glm_ortho(left, right, bottom, top, -16000, 16000, projection_matrix);
	glm_mat4_mul(view_matrix, model_matrix, mvp_matrix);
	glm_mat4_mul(projection_matrix, mvp_matrix, mvp_matrix);
	glViewport(0, 0, window_width, window_height);
}

void stop_drawing() {
	submit_batch();
	SDL_GL_SwapWindow(window);
}

bool window_maximized() {
	return get_fullscreen() || (SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED);
}

#define FOCUS_IMPOSSIBLE (SDL_WINDOW_OCCLUDED | SDL_WINDOW_MINIMIZED | SDL_WINDOW_NOT_FOCUSABLE)
#define HAS_FOCUS                                                                                                      \
	(SDL_WINDOW_MOUSE_GRABBED | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_CAPTURE | SDL_WINDOW_KEYBOARD_GRABBED)
bool window_focused() {
	const SDL_WindowFlags flags = SDL_GetWindowFlags(window);
	return !(flags & FOCUS_IMPOSSIBLE) && (flags & HAS_FOCUS);
}

bool window_start_text_input() {
	return SDL_StartTextInput(window);
}

void window_stop_text_input() {
	SDL_StopTextInput(window);
}

// =====
// STATE
// =====

void start_video_state() {
	nuke_video_state();
}

void nuke_video_state() {
	SDL_memset(&video_state, 0, sizeof(video_state));
}

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
	get_resolution(&window_width, &window_height);
}

bool get_fullscreen() {
	return SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN;
}

void set_fullscreen(bool fullscreen) {
	SDL_SetWindowFullscreen(window, fullscreen);
	SDL_SyncWindow(window);
	SDL_GetWindowSizeInPixels(window, &window_width, &window_height);
}

bool get_vsync() {
	int interval = 0;
	SDL_GL_GetSwapInterval(&interval);
	return (interval != 0);
}

void set_vsync(bool vsync) {
	SDL_GL_SetSwapInterval(vsync);
}

// =====
// BASIC
// =====

/// Clears the current render target's color buffer.
void clear_color(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
	glClearColor(r, g, b, a), glClear(GL_COLOR_BUFFER_BIT);
}

/// Clears the current render target's depth buffer.
void clear_depth(GLfloat depth) {
	glClearDepthf(depth), glClear(GL_DEPTH_BUFFER_BIT);
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
	if (!name || !*name || get_texture(name))
		return;

	Texture texture = {0};

	const char* file = find_data_file(fmt("data/textures/%s.*", name), ".json");
	if (!file) {
		WTF("Texture \"%s\" not found", name);
		return;
	}

	SDL_Surface* surface = IMG_Load(file);
	if (!surface) {
		WTF("Texture \"%s\" fail: %s", name, SDL_GetError());
		return;
	}

	texture.name = SDL_strdup(name);
	texture.size[0] = surface->w, texture.size[1] = surface->h;

	glGenTextures(1, &(texture.texture));
	glBindTexture(GL_TEXTURE_2D, texture.texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

	SDL_Surface* temp = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
	EXPECT(temp, "Texture \"%s\" conversion fail: %s", name, SDL_GetError());
	SDL_DestroySurface(surface);
	surface = temp;

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, surface->w, surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);
	SDL_DestroySurface(surface);

	file = find_data_file(fmt("data/textures/%s.json", name), NULL);
	if (file == NULL)
		goto tex_no_json;

	yyjson_doc* json = yyjson_read_file(file, JSON_READ_FLAGS, NULL, NULL);
	if (json == NULL)
		goto tex_no_json;

	yyjson_val* root = yyjson_doc_get_root(json);
	if (!yyjson_is_obj(root))
		goto tex_no_obj;

	texture.offset[0] = (GLfloat)yyjson_get_num(yyjson_obj_get(root, "x_offset"));
	texture.offset[1] = (GLfloat)yyjson_get_num(yyjson_obj_get(root, "y_offset"));

tex_no_obj:
	yyjson_doc_free(json);
tex_no_json:
	StMapPut(textures, StHashStr(name), &texture, sizeof(texture))->cleanup = nuke_texture;
	return;
}

/// Load a number of frame-indexed textures, starting at 0.
void load_texture_num(const char* pattern, uint32_t n) {
	static char buf[256] = "";
	for (uint32_t i = 0; i < n; i++) {
		SDL_snprintf(buf, sizeof(buf), pattern, i);
		load_texture(buf);
	}
}

const Texture* get_texture(const char* name) {
	return StMapGet(textures, StHashStr(name));
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
	ASSUME(file, "Font \"%s\" not found", name);

	yyjson_read_err error;
	yyjson_doc* json = yyjson_read_file(file, JSON_READ_FLAGS, NULL, &error);
	ASSUME(json, "Font \"%s\" fail: %s", name, error.msg);

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

	size_t i = 0, n = 0;
	yyjson_val *key = NULL, *value = NULL;
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

	StMapPut(fonts, StHashStr(name), &font, sizeof(font))->cleanup = nuke_texture;
}

const Font* get_font(const char* name) {
	return StMapGet(fonts, StHashStr(name));
}

// =====
// BATCH
// =====

/// Dumps the vertex batch on your screen.
void submit_batch() {
	if (batch.vertex_count <= 0)
		return;

	glBindVertexArray(batch.vao);
	glBindBuffer(GL_ARRAY_BUFFER, batch.vbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(sizeof(Vertex) * batch.vertex_count), batch.vertices);

	// Apply texture
	glBindTexture(GL_TEXTURE_2D, batch.texture);
	const GLint filter = (batch.filter || CLIENT.video.filter) ? GL_LINEAR : GL_NEAREST;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
	glUniform1f(uniforms.alpha_test, batch.alpha_test);
	glUniform4fv(uniforms.stencil, 1, batch.stencil);
	glUniformMatrix4fv(uniforms.mvp, 1, GL_FALSE, (const GLfloat*)get_mvp_matrix());

	// Apply blend mode
	glBlendFuncSeparate(batch.blend[0][0], batch.blend[0][1], batch.blend[1][0], batch.blend[1][1]);

	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)batch.vertex_count);
	batch.vertex_count = 0;
}

void batch_pos(const GLfloat pos[3]) {
	batch.pos[0] = pos[0];
	batch.pos[1] = pos[1];
	batch.pos[2] = pos[2];
}

void batch_offset(const GLfloat offset[3]) {
	batch.offset[0] = offset[0];
	batch.offset[1] = offset[1];
	batch.offset[2] = offset[2];
}

void batch_scale(const GLfloat scale[3]) {
	batch.scale[0] = scale[0];
	batch.scale[1] = scale[1];
	batch.scale[2] = scale[2];
}

void batch_angle(const GLfloat angle) {
	batch.angle = angle;
}

void batch_color(const GLubyte color[4]) {
	SDL_memcpy(batch.color[0], color, sizeof(batch.color[0]));
	SDL_memcpy(batch.color[1], color, sizeof(batch.color[1]));
	SDL_memcpy(batch.color[2], color, sizeof(batch.color[2]));
	SDL_memcpy(batch.color[3], color, sizeof(batch.color[3]));
}

void batch_colors(const GLubyte colors[4][4]) {
	SDL_memcpy(batch.color, colors, sizeof(batch.color));
}

void batch_stencil(const GLfloat stencil[4]) {
	if (SDL_memcmp(batch.stencil, stencil, sizeof(batch.stencil)))
		submit_batch();
	SDL_memcpy(batch.stencil, stencil, sizeof(batch.stencil));
}

void batch_flip(const GLboolean flip[2]) {
	batch.flip[0] = flip[0];
	batch.flip[1] = flip[1];
}

void batch_tile(const GLboolean tile[2]) {
	batch.tile[0] = tile[0];
	batch.tile[1] = tile[1];
}

void batch_align(const FontAlignment align[2]) {
	batch.align[0] = align[0];
	batch.align[1] = align[1];
}

static void batch_texture(GLuint texture) {
	if (batch.texture != texture)
		submit_batch();
	batch.texture = texture;
}

void batch_filter(bool filter) {
	if (batch.filter != filter)
		submit_batch();
	batch.filter = filter;
}

void batch_alpha_test(GLfloat alpha_test) {
	if (batch.alpha_test != alpha_test)
		submit_batch();
	batch.alpha_test = alpha_test;
}

void batch_blend(const GLenum blend[2][2]) {
	if (SDL_memcmp(batch.blend, blend, sizeof(batch.blend)))
		submit_batch();
	SDL_memcpy(batch.blend, blend, sizeof(batch.blend));
}

/// Resets the batch.
void batch_reset() {
	batch_pos(B_ORIGIN), batch_offset(B_ORIGIN), batch_scale(B_XYZ(1.f, 1.f, 1.f)), batch_angle(0.f);
	batch_color(B_WHITE);
	batch_flip(B_NO_FLIP), batch_tile(B_NO_TILE), batch_align(B_TOP_LEFT);
}

/// Fully resets the batch, slower since it can break batches.
void batch_reset_hard() {
	batch_reset();
	batch_texture(blank_texture), batch_filter(true), batch_alpha_test(0.5f);
	batch_stencil(B_NO_STENCIL), batch_blend(B_BLEND_NORMAL);
}

/// Adds a vertex to the batch.
static void batch_vertex(const GLfloat pos[3], const GLubyte color[4], const GLfloat uv[2]) {
	if (batch.vertex_count < batch.vertex_capacity)
		goto just_push;

	submit_batch();

	const size_t new_size = batch.vertex_capacity * 2;
	EXPECT(new_size >= batch.vertex_capacity, "Capacity overflow in vertex batch");
	batch.vertices = SDL_realloc(batch.vertices, new_size * sizeof(Vertex));
	EXPECT(batch.vertices, "Out of memory for vertex batch");

	batch.vertex_capacity = new_size;

	glBindBuffer(GL_ARRAY_BUFFER, batch.vbo);
	glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(Vertex) * batch.vertex_capacity), NULL, GL_DYNAMIC_DRAW);

just_push:
	batch.vertices[batch.vertex_count++] = (Vertex){
		.position = {pos[0], pos[1], pos[2]},
		.color = {color[0], color[1], color[2], color[3]},
		.uv = {uv[0], uv[1]},
	};
}

/// Adds a sprite to the batch.
void batch_sprite(const char* name) {
	const Texture* texture = get_texture(name);
	if (texture == NULL)
		return;

	batch_texture(texture->texture);

	// Position
	const GLfloat w = (GLfloat)texture->size[0] * batch.scale[0], h = (GLfloat)texture->size[1] * batch.scale[1];

	const GLfloat xoffs = (texture->offset[0] + batch.offset[0]) * batch.scale[0],
		      yoffs = (texture->offset[1] + batch.offset[1]) * batch.scale[1];

	const GLfloat x1 = -(batch.flip[0] ? (w - xoffs) : xoffs), y1 = -(batch.flip[1] ? (h - yoffs) : yoffs);
	const GLfloat x2 = x1 + w, y2 = y1 + h;
	const GLfloat z = batch.offset[2] * batch.scale[2];

	vec3 p1 = {x1, y1, z}, p2 = {x2, y1, z}, p3 = {x1, y2, z}, p4 = {x2, y2, z};
	if (batch.angle != 0.f)
		glm_vec2_rotate(p1, batch.angle, p1), glm_vec2_rotate(p2, batch.angle, p2),
			glm_vec2_rotate(p3, batch.angle, p3), glm_vec2_rotate(p4, batch.angle, p4);
	glm_vec3_add(batch.pos, p1, p1), glm_vec3_add(batch.pos, p2, p2), glm_vec3_add(batch.pos, p3, p3),
		glm_vec3_add(batch.pos, p4, p4);

	// UVs
	const GLfloat u1 = batch.flip[0], v1 = batch.flip[1];
	const GLfloat u2 = (GLfloat)(!batch.flip[0]), v2 = (GLfloat)(!batch.flip[1]);

	// Vertices
	batch_vertex(B_XYZ(p3[0], p3[1], p3[2]), batch.color[2], B_UV(u1, v2));
	batch_vertex(B_XYZ(p1[0], p1[1], p1[2]), batch.color[0], B_UV(u1, v1));
	batch_vertex(B_XYZ(p2[0], p2[1], p2[2]), batch.color[1], B_UV(u2, v1));
	batch_vertex(B_XYZ(p2[0], p2[1], p2[2]), batch.color[1], B_UV(u2, v1));
	batch_vertex(B_XYZ(p4[0], p4[1], p4[2]), batch.color[3], B_UV(u2, v2));
	batch_vertex(B_XYZ(p3[0], p3[1], p3[2]), batch.color[2], B_UV(u1, v2));
}

/// Adds a surface to the vertex batch.
void batch_surface(Surface* surface) {
	if (surface == NULL)
		return;
	EXPECT(!surface->active, "Drawing an active surface?");
	if (surface->texture[SURF_COLOR] == 0)
		return;

	batch_texture(surface->texture[SURF_COLOR]);

	// Position
	const GLfloat w = (GLfloat)surface->size[0] * batch.scale[0], h = (GLfloat)surface->size[1] * batch.scale[1];

	const GLfloat xoffs = batch.offset[0] * batch.scale[0], yoffs = batch.offset[1] * batch.scale[1];

	const GLfloat x1 = -(batch.flip[0] ? (w - xoffs) : xoffs), y1 = -(batch.flip[1] ? (h - yoffs) : yoffs);
	const GLfloat x2 = x1 + w, y2 = y1 + h;
	const GLfloat z = batch.offset[2] * batch.scale[2];

	vec3 p1 = {x1, y1, z}, p2 = {x2, y1, z}, p3 = {x1, y2, z}, p4 = {x2, y2, z};
	if (batch.angle != 0.f)
		glm_vec2_rotate(p1, batch.angle, p1), glm_vec2_rotate(p2, batch.angle, p2),
			glm_vec2_rotate(p3, batch.angle, p3), glm_vec2_rotate(p4, batch.angle, p4);
	glm_vec3_add(batch.pos, p1, p1), glm_vec3_add(batch.pos, p2, p2), glm_vec3_add(batch.pos, p3, p3),
		glm_vec3_add(batch.pos, p4, p4);

	// UVs
	const GLfloat u1 = batch.flip[0], v1 = batch.flip[1];
	const GLfloat u2 = (GLfloat)(!batch.flip[0]), v2 = (GLfloat)(!batch.flip[1]);

	// Vertices
	batch_vertex(B_XYZ(p3[0], p3[1], p3[2]), batch.color[2], B_UV(u1, v2));
	batch_vertex(B_XYZ(p1[0], p1[1], p1[2]), batch.color[0], B_UV(u1, v1));
	batch_vertex(B_XYZ(p2[0], p2[1], p2[2]), batch.color[1], B_UV(u2, v1));
	batch_vertex(B_XYZ(p2[0], p2[1], p2[2]), batch.color[1], B_UV(u2, v1));
	batch_vertex(B_XYZ(p4[0], p4[1], p4[2]), batch.color[3], B_UV(u2, v2));
	batch_vertex(B_XYZ(p3[0], p3[1], p3[2]), batch.color[2], B_UV(u1, v2));
}

/// Adds a rectangle to the batch.
void batch_rectangle(const char* name, const GLfloat size[2]) {
	const Texture* texture = get_texture(name);
	batch_texture(texture == NULL ? blank_texture : texture->texture);

	GLfloat tw = 1.f, th = 1.f;
	if (texture != NULL)
		tw = (GLfloat)texture->size[0] * batch.scale[0], th = (GLfloat)texture->size[1] * batch.scale[1];

	// Position
	const GLfloat w = size[0] * batch.scale[0], h = size[1] * batch.scale[1];
	const GLfloat xoffs = batch.offset[0] * batch.scale[0], yoffs = batch.offset[1] * batch.scale[1];
	const GLfloat x1 = -(batch.flip[0] ? (w - xoffs) : xoffs), y1 = -(batch.flip[1] ? (h - yoffs) : yoffs);
	const GLfloat x2 = x1 + w, y2 = y1 + h;
	const GLfloat z = batch.offset[2] * batch.scale[2];

	vec3 p1 = {x1, y1, z}, p2 = {x2, y1, z}, p3 = {x1, y2, z}, p4 = {x2, y2, z};
	if (batch.angle != 0.f)
		glm_vec2_rotate(p1, batch.angle, p1), glm_vec2_rotate(p2, batch.angle, p2),
			glm_vec2_rotate(p3, batch.angle, p3), glm_vec2_rotate(p4, batch.angle, p4);
	glm_vec3_add(batch.pos, p1, p1), glm_vec3_add(batch.pos, p2, p2), glm_vec3_add(batch.pos, p3, p3),
		glm_vec3_add(batch.pos, p4, p4);

	// UVs
	GLfloat u1 = 0.f, v1 = 0.f, u2 = 1.f, v2 = 1.f;
	if (batch.tile[0])
		u2 = (batch.flip[0] ? -1.f : 1.f) * ((x2 - x1) / tw);
	else
		u1 = batch.flip[0], u2 = (GLfloat)(!batch.flip[0]);
	if (batch.tile[1])
		v2 = (batch.flip[1] ? -1.f : 1.f) * ((y2 - y1) / th);
	else
		v1 = batch.flip[1], v2 = (GLfloat)(!batch.flip[1]);

	// Vertices
	batch_vertex(B_XYZ(p3[0], p3[1], p3[2]), batch.color[2], B_UV(u1, v2));
	batch_vertex(B_XYZ(p1[0], p1[1], p1[2]), batch.color[0], B_UV(u1, v1));
	batch_vertex(B_XYZ(p2[0], p2[1], p2[2]), batch.color[1], B_UV(u2, v1));
	batch_vertex(B_XYZ(p2[0], p2[1], p2[2]), batch.color[1], B_UV(u2, v1));
	batch_vertex(B_XYZ(p4[0], p4[1], p4[2]), batch.color[3], B_UV(u2, v2));
	batch_vertex(B_XYZ(p3[0], p3[1], p3[2]), batch.color[2], B_UV(u1, v2));
}

static GLfloat string_width_fast(const Font* font, GLfloat size, const char* str) {
	if (font == NULL)
		return 0;

	GLfloat width = 0;
	GLfloat cx = 0;

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
		cx += font->glyphs[gid].width;
		if (bytes > 0)
			cx += font->spacing;

		width = SDL_max(width, cx);
	}

	return width * (size / font->height);
}

GLfloat string_width(const char* name, GLfloat size, const char* str) {
	return string_width_fast(get_font(name), size, str);
}

static GLfloat string_height_fast(const Font* font, GLfloat size, const char* str) {
	if (font == NULL)
		return 0;

	size_t bytes = SDL_strlen(str);
	GLfloat height = (bytes > 0) ? font->height : 0;
	while (bytes > 0)
		if (SDL_StepUTF8(&str, &bytes) == '\n')
			height += font->height;

	return height * (size / font->height);
}

GLfloat string_height(const char* name, GLfloat size, const char* str) {
	return string_height_fast(get_font(name), size, str);
}

/// Adds a string to the vertex batch.
void batch_string(const char* font_name, GLfloat size, const char* str) {
	const Font* font = get_font(font_name);
	if (font == NULL || str == NULL)
		return;

	batch_texture(font->texture->texture);

	// Origin
	GLfloat ox = batch.pos[0] + (batch.offset[0] * batch.scale[0]);
	GLfloat oy = batch.pos[1] + (batch.offset[1] * batch.scale[1]);
	GLfloat oz = batch.pos[2] + (batch.offset[2] * batch.scale[2]);

	// Horizontal alignment
	switch (batch.align[0]) {
	case FA_CENTER:
		ox -= (GLfloat)((GLint)(string_width_fast(font, size, str) / 2.f)) * batch.scale[0];
		break;
	case FA_RIGHT:
		ox -= (string_width_fast(font, size, str)) * batch.scale[0];
		break;
	default:
		break;
	}

	// Vertical alignment
	switch (batch.align[1]) {
	case FA_MIDDLE:
		oy -= (GLfloat)((GLint)(string_height_fast(font, size, str) / 2.f)) * batch.scale[1];
		break;
	case FA_BOTTOM:
		oy -= (string_height_fast(font, size, str)) * batch.scale[1];
		break;
	default:
		break;
	}

	GLfloat cx = ox, cy = oy;
	const GLfloat scale = size / font->height, spacing = font->spacing * scale * batch.scale[0],
		      height = size * batch.scale[1];

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
		const GLfloat width = glyph->width * scale * batch.scale[0];

		const GLfloat x1 = cx;
		const GLfloat y1 = cy;
		const GLfloat x2 = x1 + width;
		const GLfloat y2 = y1 + height;
		batch_vertex(B_XYZ(x1, y2, oz), batch.color[2], B_UV(glyph->uvs[0], glyph->uvs[3]));
		batch_vertex(B_XYZ(x1, y1, oz), batch.color[0], B_UV(glyph->uvs[0], glyph->uvs[1]));
		batch_vertex(B_XYZ(x2, y1, oz), batch.color[1], B_UV(glyph->uvs[2], glyph->uvs[1]));
		batch_vertex(B_XYZ(x2, y1, oz), batch.color[1], B_UV(glyph->uvs[2], glyph->uvs[1]));
		batch_vertex(B_XYZ(x2, y2, oz), batch.color[3], B_UV(glyph->uvs[2], glyph->uvs[3]));
		batch_vertex(B_XYZ(x1, y2, oz), batch.color[2], B_UV(glyph->uvs[0], glyph->uvs[3]));

		cx += width;
		if (bytes > 0)
			cx += spacing;
	}
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
	EXPECT(surface, "create_surface fail");
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
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

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
	EXPECT(!surface->active, "Disposing an active surface?");

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
	EXPECT(!surface->active, "Resizing an active surface?");
	if (surface->size[0] == width && surface->size[1] == height)
		return;
	dispose_surface(surface);
	surface->size[0] = width;
	surface->size[1] = height;
}

/// Pushes an INACTIVE surface onto the stack.
void push_surface(Surface* surface) {
	EXPECT(surface, "Pushing a null surface?");
	EXPECT(current_surface != surface, "Pushing the current surface?");
	submit_batch();

	if (surface == NULL) {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
		glDisable(GL_DEPTH_TEST);
		glCullFace(GL_FRONT);
	} else {
		EXPECT(!surface->active, "Pushing an active surface?");
		surface->active = true;
		surface->previous = current_surface;

		check_surface(surface);
		glBindFramebuffer(GL_FRAMEBUFFER, surface->fbo);
		glViewport(0, 0, (GLsizei)(surface->size[0]), (GLsizei)(surface->size[1]));
		(surface->enabled[SURF_DEPTH] ? glEnable : glDisable)(GL_DEPTH_TEST);
		glCullFace(GL_BACK);
	}

	current_surface = surface;
}

/// Pops an ACTIVE surface off the stack.
void pop_surface() {
	EXPECT(current_surface, "Popping a null surface?");
	submit_batch();

	Surface* surface = current_surface->previous;
	current_surface->active = false;
	current_surface->previous = NULL;

	if (surface == NULL) {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, window_width, window_height);
		glDisable(GL_DEPTH_TEST);
		glCullFace(GL_FRONT);
	} else {
		check_surface(surface);
		glBindFramebuffer(GL_FRAMEBUFFER, surface->fbo);
		glViewport(0, 0, (GLsizei)(surface->size[0]), (GLsizei)(surface->size[1]));
		(surface->enabled[SURF_DEPTH] ? glEnable : glDisable)(GL_DEPTH_TEST);
		glCullFace(GL_BACK);
	}

	current_surface = surface;
}

// =====
// TILES
// =====

static void nuke_tilemap(void* ptr) {
	struct TileMap* tilemap = (struct TileMap*)ptr;
	if (last_tilemap == tilemap)
		last_tilemap = tilemap->next;
	glDeleteVertexArrays(1, &(tilemap->vao));
	glDeleteBuffers(1, &(tilemap->vbo));
	SDL_free(tilemap->vertices);
}

static TileMap* fetch_tilemap(const char* name) {
	const StTinyKey key = StHashStr(name);
	TileMap* ptr = StMapGet(tilemaps, key);
	if (ptr != NULL)
		return ptr;

	TileMap tilemap = {0};

	tilemap.next = last_tilemap;
	if (name == NULL)
		tilemap.texture = NULL;
	else {
		load_texture(name);
		tilemap.texture = get_texture(name);
	}
	tilemap.translucent = false;

	glGenVertexArrays(1, &(tilemap.vao));
	glBindVertexArray(tilemap.vao);
	glEnableVertexArrayAttrib(tilemap.vao, VATT_POSITION);
	glVertexArrayAttribFormat(tilemap.vao, VATT_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 3);
	glEnableVertexArrayAttrib(tilemap.vao, VATT_COLOR);
	glVertexArrayAttribFormat(tilemap.vao, VATT_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(GLubyte) * 4);
	glEnableVertexArrayAttrib(tilemap.vao, VATT_UV);
	glVertexArrayAttribFormat(tilemap.vao, VATT_UV, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 2);

	tilemap.vertex_count = 0;
	tilemap.vertex_capacity = 6;
	tilemap.vertices = SDL_malloc(tilemap.vertex_capacity * sizeof(Vertex));
	EXPECT(tilemap.vertices, "Out of memory for tilemap \"%s\" vertices", name);

	glGenBuffers(1, &(tilemap.vbo));
	glBindBuffer(GL_ARRAY_BUFFER, tilemap.vbo);
	glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(Vertex) * tilemap.vertex_capacity), NULL, GL_STATIC_DRAW);
	glEnableVertexAttribArray(VATT_POSITION);
	glVertexAttribPointer(VATT_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
	glEnableVertexAttribArray(VATT_COLOR);
	glVertexAttribPointer(VATT_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex, color));
	glEnableVertexAttribArray(VATT_UV);
	glVertexAttribPointer(VATT_UV, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));

	StTinyBucket* bucket = StMapPut(tilemaps, key, &tilemap, sizeof(tilemap));
	bucket->cleanup = nuke_tilemap;
	return (last_tilemap = bucket->data);
}

static void tile_vertex(TileMap* tilemap, const GLfloat pos[3], const GLubyte color[4], const GLfloat uv[2]) {
	if (tilemap->vertex_count >= tilemap->vertex_capacity) {
		const size_t new_size = tilemap->vertex_capacity * 2;
		if (new_size < tilemap->vertex_capacity)
			FATAL("Capacity overflow in tilemap %p", tilemap);

		tilemap->vertices = SDL_realloc(tilemap->vertices, new_size * sizeof(Vertex));
		if (tilemap->vertices == NULL)
			FATAL("Out of memory for tilemap %p vertices", tilemap);

		tilemap->vertex_capacity = new_size;

		glBindBuffer(GL_ARRAY_BUFFER, tilemap->vbo);
		glBufferData(
			GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(Vertex) * tilemap->vertex_capacity), NULL, GL_STATIC_DRAW);
	}

	tilemap->vertices[tilemap->vertex_count++]
		= (Vertex){pos[0], pos[1], pos[2], color[0], color[1], color[2], color[3], uv[0], uv[1]};
}

void tile_sprite(const char* name, const GLfloat pos[3], const GLfloat scale[2], const GLubyte color[4]) {
	TileMap* tilemap = fetch_tilemap(name);
	const Texture* texture = tilemap->texture;
	if (texture == NULL)
		return;

	const GLfloat x1 = pos[0] - (texture->offset[0] * scale[0]);
	const GLfloat y1 = pos[1] - (texture->offset[1] * scale[1]);
	const GLfloat x2 = x1 + ((GLfloat)texture->size[0] * scale[0]);
	const GLfloat y2 = y1 + ((GLfloat)texture->size[1] * scale[1]);
	const GLfloat z = pos[2];

	tile_vertex(tilemap, B_XYZ(x1, y2, z), color, B_UV(0, 1));
	tile_vertex(tilemap, B_XYZ(x1, y1, z), color, B_UV(0, 0));
	tile_vertex(tilemap, B_XYZ(x2, y1, z), color, B_UV(1, 0));
	tile_vertex(tilemap, B_XYZ(x2, y1, z), color, B_UV(1, 0));
	tile_vertex(tilemap, B_XYZ(x2, y2, z), color, B_UV(1, 1));
	tile_vertex(tilemap, B_XYZ(x1, y2, z), color, B_UV(0, 1));

	if (color[3] < 255)
		tilemap->translucent = true;
}

void tile_rectangle(const char* name, const GLfloat rect[2][2], GLfloat z, const GLubyte color[4][4]) {
	TileMap* tilemap = fetch_tilemap(name);
	const Texture* texture = tilemap->texture;
	const GLfloat u = (texture != NULL) ? ((rect[1][0] - rect[0][0]) / (GLfloat)(texture->size[0])) : 1;
	const GLfloat v = (texture != NULL) ? ((rect[1][1] - rect[0][1]) / (GLfloat)(texture->size[1])) : 1;

	tile_vertex(tilemap, B_XYZ(rect[0][0], rect[1][1], z), color[2], B_UV(0, v));
	tile_vertex(tilemap, B_XYZ(rect[0][0], rect[0][1], z), color[0], B_UV(0, 0));
	tile_vertex(tilemap, B_XYZ(rect[1][0], rect[0][1], z), color[1], B_UV(u, 0));
	tile_vertex(tilemap, B_XYZ(rect[1][0], rect[0][1], z), color[1], B_UV(u, 0));
	tile_vertex(tilemap, B_XYZ(rect[1][0], rect[1][1], z), color[3], B_UV(u, v));
	tile_vertex(tilemap, B_XYZ(rect[0][0], rect[1][1], z), color[2], B_UV(0, v));

	if (color[0][3] < 255 || color[1][3] < 255 || color[2][3] < 255 || color[3][3] < 255)
		tilemap->translucent = true;
}

void draw_tilemaps() {
	TileMap* tilemap = last_tilemap;
	if (tilemap == NULL)
		return;

	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE);
	glUniform1f(uniforms.alpha_test, 0.5f);
	glUniformMatrix4fv(uniforms.mvp, 1, GL_FALSE, (const GLfloat*)get_mvp_matrix());
	while (tilemap != NULL) {
		glDepthMask(tilemap->translucent ? GL_FALSE : GL_TRUE);
		glBindVertexArray(tilemap->vao);
		glBindBuffer(GL_ARRAY_BUFFER, tilemap->vbo);
		glBufferSubData(
			GL_ARRAY_BUFFER, 0, (GLsizeiptr)(sizeof(Vertex) * tilemap->vertex_count), tilemap->vertices);

		glBindTexture(GL_TEXTURE_2D, (tilemap->texture == NULL) ? blank_texture : tilemap->texture->texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glDrawArrays(GL_TRIANGLES, 0, (GLsizei)tilemap->vertex_count);

		tilemap = tilemap->next;
	}

	glDepthMask(GL_TRUE);
}

void clear_tilemaps() {
	FreeTinyMap(tilemaps);
	tilemaps = NewTinyMap();
	last_tilemap = NULL;
}

// ======
// CAMERA
// ======

void lerp_camera(float ticks) {
	VideoCamera* camera = &video_state.camera;
	glm_vec2_copy(camera->pos, camera->from);
	camera->lerp_time[0] = 0.f;
	camera->lerp_time[1] = ticks;
}
