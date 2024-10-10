#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"
#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"
#include "hex_dump.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include <random>
#include <array>

GLuint sea_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > sea_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("sea.pnct"));
	sea_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > sea_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("sea.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = sea_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = sea_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;
		

	});
});


GLuint octo_meshes_for_lit_color_texture_program = 1;
Load< MeshBuffer > normal_octo_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("octo.pnct"));
	octo_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

// GLuint inked_octo_meshes_for_lit_color_texture_program = 2;
// Load< MeshBuffer > inked_octo_meshes(LoadTagDefault, []() -> MeshBuffer const * {
// 	MeshBuffer const *ret = new MeshBuffer(data_path("inkedocto.pnct"));
// 	inked_octo_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
// 	return ret;
// });
// probably want something like above for each octopus too

PlayMode::PlayMode(Client &client_) : client(client_) {
	scene = *sea_scene;
	for (auto &transform : scene.transforms) {
		if (transform.name.rfind("shell")==0) {
			shells.emplace_back(&transform);
			}
	}

	//create a camera
	scene.transforms.emplace_back();
	scene.cameras.emplace_back(&scene.transforms.back());
	camera = &scene.cameras.back();
	camera->transform->position = glm::vec3(0,0,20); 

	// create player's octopus
	Mesh const &mesh = normal_octo_meshes->lookup("Sphere");
	Scene::Transform *transform = new Scene::Transform();
	scene.drawables.emplace_back(transform);
	Scene::Drawable &drawable = scene.drawables.back();

	drawable.pipeline = lit_color_texture_program_pipeline;
	drawable.pipeline.vao = octo_meshes_for_lit_color_texture_program;
	drawable.pipeline.type = mesh.type;
	drawable.pipeline.start = mesh.start;
	drawable.pipeline.count = mesh.count;
	
	drawable.transform->position = glm::vec3(game.players.front().position.x,game.players.front().position.y,1.0f);
	playerocto = transform;
	
	lastdrawable = scene.drawables.size();


}



PlayMode::~PlayMode() {
}


bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.repeat) {
			//ignore repeats
		} else if (evt.key.keysym.sym == SDLK_a) {
			controls.left.downs += 1;
			controls.left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			controls.right.downs += 1;
			controls.right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			controls.up.downs += 1;
			controls.up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			controls.down.downs += 1;
			controls.down.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_SPACE) {
			controls.ink.downs += 1;
			controls.ink.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a) {
			controls.left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			controls.right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			controls.up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			controls.down.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_SPACE) {
			controls.ink.pressed = false;
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {

	//queue data for sending to server:
	controls.send_controls_message(&client.connection);

	//reset button press counters:
	controls.left.downs = 0;
	controls.right.downs = 0;
	controls.up.downs = 0;
	controls.down.downs = 0;
	controls.ink.downs = 0;

	//send/receive data:
	client.poll([this](Connection *c, Connection::Event event){
		if (event == Connection::OnOpen) {
			std::cout << "[" << c->socket << "] opened" << std::endl;
		} else if (event == Connection::OnClose) {
			std::cout << "[" << c->socket << "] closed (!)" << std::endl;
			throw std::runtime_error("Lost connection to server!");
		} else { assert(event == Connection::OnRecv);
			//std::cout << "[" << c->socket << "] recv'd data. Current buffer:\n" << hex_dump(c->recv_buffer); std::cout.flush(); //DEBUG
			bool handled_message;
			try {
				do {
					handled_message = false;
					if (game.recv_state_message(c)) handled_message = true;
				} while (handled_message);
			} catch (std::exception const &e) {
				std::cerr << "[" << c->socket << "] malformed message from server: " << e.what() << std::endl;
				//quit the game:
				throw e;
			}
		}
	}, 0.0);

	playerocto->position = glm::vec3(game.players.front().position.x ,game.players.front().position.y ,2.0f);
	for (size_t i = 0;i< game.shells.size();i++){
		shells[i]->position = glm::vec3(game.shells[i].x,game.shells[i].y,1.0f);
	}
	// scene = *sea_scene;
	// scene.drawables.resize(lastdrawable,NULL);
	// for (auto &p1 : game.players) {
	// 	if (&p1==&game.players.front()){continue;}
	// 	if (p1.inked){
	// 		// create other player's octopus
	// 		Mesh mesh = inked_octo_meshes->lookup("Sphere.001");
	// 		auto transform = new Scene::Transform();
	// 		scene.drawables.emplace_back(transform);
	// 		Scene::Drawable &drawable = scene.drawables.back();

	// 		drawable.pipeline = lit_color_texture_program_pipeline;
	// 		drawable.pipeline.vao = inked_octo_meshes_for_lit_color_texture_program;
	// 		drawable.pipeline.type = mesh.type;
	// 		drawable.pipeline.start = mesh.start;
	// 		drawable.pipeline.count = mesh.count;
			
	// 		drawable.transform->position = glm::vec3(p1.position.x,p1.position.y,1.0f);
	// 	}
	// }

}

void PlayMode::draw(glm::uvec2 const &drawable_size) {

	camera->aspect = float(drawable_size.x) / float(drawable_size.y);
	camera->transform->position = glm::vec3(0,0,20); 

	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 1.0f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	GL_ERRORS(); //print any errors produced by this setup code

	scene.draw(*camera);
	// static std::array< glm::vec2, 16 > const circle = [](){
	// 	std::array< glm::vec2, 16 > ret;
	// 	for (uint32_t a = 0; a < ret.size(); ++a) {
	// 		float ang = a / float(ret.size()) * 2.0f * float(M_PI);
	// 		ret[a] = glm::vec2(std::cos(ang), std::sin(ang));
	// 	}
	// 	return ret;
	// }();

	// glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	// glClear(GL_COLOR_BUFFER_BIT);
	// glDisable(GL_DEPTH_TEST);
	
	//figure out view transform to center the arena:
	float aspect = float(drawable_size.x) / float(drawable_size.y);
	float scale = std::min(
		2.0f * aspect / (Game::ArenaMax.x - Game::ArenaMin.x + 2.0f * Game::PlayerRadius),
		2.0f / (Game::ArenaMax.y - Game::ArenaMin.y + 2.0f * Game::PlayerRadius)
	);
	glm::vec2 offset = -0.5f * (Game::ArenaMax + Game::ArenaMin);

	glm::mat4 world_to_clip = glm::mat4(
		scale / aspect, 0.0f, 0.0f, offset.x,
		0.0f, scale, 0.0f, offset.y,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);

	{
		DrawLines lines(world_to_clip);

		//helper:
		auto draw_text = [&](glm::vec2 const &at, std::string const &text, float H) {
			lines.draw_text(text,
				glm::vec3(at.x, at.y, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0x00, 0x00, 0x00, 0x00));
			// float ofs = (1.0f / scale) / drawable_size.y;
			// lines.draw_text(text,
			// 	glm::vec3(at.x + ofs, at.y + ofs, 0.0),
			// 	glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			// 	glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		};

		// lines.draw(glm::vec3(Game::ArenaMin.x, Game::ArenaMin.y, 0.0f), glm::vec3(Game::ArenaMax.x, Game::ArenaMin.y, 0.0f), glm::u8vec4(0xff, 0x00, 0xff, 0xff));
		// lines.draw(glm::vec3(Game::ArenaMin.x, Game::ArenaMax.y, 0.0f), glm::vec3(Game::ArenaMax.x, Game::ArenaMax.y, 0.0f), glm::u8vec4(0xff, 0x00, 0xff, 0xff));
		// lines.draw(glm::vec3(Game::ArenaMin.x, Game::ArenaMin.y, 0.0f), glm::vec3(Game::ArenaMin.x, Game::ArenaMax.y, 0.0f), glm::u8vec4(0xff, 0x00, 0xff, 0xff));
		// lines.draw(glm::vec3(Game::ArenaMax.x, Game::ArenaMin.y, 0.0f), glm::vec3(Game::ArenaMax.x, Game::ArenaMax.y, 0.0f), glm::u8vec4(0xff, 0x00, 0xff, 0xff));

		// for (auto const &player : game.players) { 
		// 	glm::u8vec4 col = glm::u8vec4(1.0*255, 1.0*255, 1.0*255, 0xff);
		// 	if (&player == &game.players.front()) {
		// 		//mark current player (which server sends first):
		// 		lines.draw(
		// 			glm::vec3(player.position + Game::PlayerRadius * glm::vec2(-0.5f,-0.5f), 0.0f),
		// 			glm::vec3(player.position + Game::PlayerRadius * glm::vec2( 0.5f, 0.5f), 0.0f),
		// 			col
		// 		);
		// 		lines.draw(
		// 			glm::vec3(player.position + Game::PlayerRadius * glm::vec2(-0.5f, 0.5f), 0.0f),
		// 			glm::vec3(player.position + Game::PlayerRadius * glm::vec2( 0.5f,-0.5f), 0.0f),
		// 			col
		// 		);
		// 	}
		// 	for (uint32_t a = 0; a < circle.size(); ++a) {
		// 		lines.draw(
		// 			glm::vec3(player.position + Game::PlayerRadius * circle[a], 0.0f),
		// 			glm::vec3(player.position + Game::PlayerRadius * circle[(a+1)%circle.size()], 0.0f),
		// 			col
		// 		);
		// 	}

		// 	draw_text(player.position + glm::vec2(0.0f, -0.1f + Game::PlayerRadius), player.name, 0.09f);
		// 	// draw a score board
		// }
		draw_text(glm::vec2(12.0f,10.0f), "You are "+ game.players.front().name, 1.0f);
		float offsetscore = 1.5f;
		for (auto const &player : game.players) { 
			draw_text(glm::vec2(12.0f,10.0f-offsetscore), player.name + " Score: " + std::to_string(player.collected), 1.0f);
			offsetscore+=1.2f;
		}
			
		// draw that we are which player
		// what is our ink cooldown
		// game.players.front().cooldown
	}
	GL_ERRORS();
}
