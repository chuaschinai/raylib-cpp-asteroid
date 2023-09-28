#include <iostream>
#include <string>
#include <vector>
#include "raylib.h"
#include "raymath.h"

#define WINDOW_WIDTH			1024
#define WINDOW_HEIGHT			768
#define COLOR_BACKGROUND 		Color { 19, 19, 19, 255 }
#define COLOR_LINES 			Color { 230, 230, 230, 255 }
#define COLOR_BULLET 			Color { 200, 60, 60, 255 }
#define ASTEROID_START_NUMBER	8
#define PLAYER_ROTATION_LIMIT 	3.0f
#define PLAYER_SPEED_LIMIT 		3.0f
#define PLAYER_TIME_INVUL		250

inline float RandRangeF(float min, float max) {
	float r = rand() / (float)RAND_MAX;
	return(min + r * (max - min));
}

inline int Choose(int a, int b) {
	return((rand() > 0.5f) ? b : a);
}

typedef struct {
	float radians;
	float distance;
} PolarPoint;

/* ==== Pool ==== */
template <typename T>
struct Pool {
	T objects[50];
	T* getObject();
	void kill(T* object);
	int number_actives = 0;
};

template <typename T>
T* Pool<T>::getObject() {
	for(auto &object: objects) {
		if(!object.active) {
			object.active = true;
			number_actives++;
			return &object;
		}
	}
	return NULL;
}

template <typename T>
void Pool<T>::kill(T* object) {
	object->active = false;
	number_actives--;
}

/* ==== Particles ==== */
struct Particle {
	bool active = false;
	Vector2 pos = { 0.0f, 0.0f };
	Vector2 vel = { 0.0f, 0.0f };
	float rot = 0;
	float rot_speed;
	Color color = COLOR_LINES;
	int size;
	int life_start;
	int life_current;
};

void ParticleBurst(Pool<Particle>* pool, float x, float y, int amount) {
	do {	
		Particle* particle = pool->getObject();
		if(NULL == particle) return;
		particle->pos.x = x;
		particle->pos.y = y;
		particle->vel.x = RandRangeF(-1.0f, 1.0f);
		particle->vel.y = RandRangeF(-1.0f, 1.0f);
		particle->life_start = GetRandomValue(60, 120);
		particle->life_current = particle->life_start;
		particle->size = GetRandomValue(4, 12);
		particle->rot_speed = RandRangeF(-2.0f, 2.0f);
	}while(--amount > 0);
}

void ParticleUpdate(Pool<Particle>* pool) {
	for(auto &particle: pool->objects) {
		if(!particle.active) continue;
		particle.pos = Vector2Add(particle.pos, particle.vel);
		particle.rot += particle.rot_speed;
		particle.life_current--;
		if(particle.life_current <= 0) {
			pool->kill(&particle);
		}
	}
}

void ParticleDraw(Pool<Particle>* pool) {
	for(auto particle: pool->objects) {
		if(!particle.active) continue;
		particle.color = ColorAlpha(particle.color, (float)particle.life_current / (float)particle.life_start);
		DrawPolyLines(particle.pos, 3, particle.size, particle.rot, particle.color);
	}
}

/* ==== Bullet ==== */
struct Bullet {
	bool active = false;
	Vector2 pos = { 0, 0 };
	Vector2 vel = { 0, 0 };
};

/* ==== Player ==== */
struct Player {
	Vector2 pos = { 0.0f, 0.0f };
	Vector2 vel = { 0.0f, 0.0f };
	Vector2 shape[3];
	Color color = COLOR_LINES;
	float base_height { 45.0f };
	float rot = 0.0f;
	float rot_vel = 0.0f;
	float bullet_speed_ratio = { 7.0f };
	int lives = 3;
	int safe_mode_count;
	bool safe_mode = false;
	void update();
	void draw();
	void kill();
};

void Player::update() {
	color = BLUE;

	// movement
	vel = Vector2ClampValue(vel, -PLAYER_SPEED_LIMIT, PLAYER_SPEED_LIMIT);
	pos = Vector2Add(pos, vel);

	// rotation vel
	rot_vel = Clamp(rot_vel, -PLAYER_ROTATION_LIMIT, PLAYER_ROTATION_LIMIT);
	rot += rot_vel;
	rot_vel += (0.0f - rot_vel) * 0.05f;
	if(abs(0.0f - rot_vel) <= 0.01f) rot_vel = 0.0f;

	{	// refresh shape
		shape[0].x = pos.x + cosf(rot*DEG2RAD) * (base_height / 2);
		shape[0].y = pos.y + sinf(rot*DEG2RAD) * (base_height / 2);

		shape[1].x = pos.x + cosf((rot-135)*DEG2RAD) * (base_height / 2);
		shape[1].y = pos.y + sinf((rot-135)*DEG2RAD) * (base_height / 2);

		shape[2].x = pos.x + cosf((rot-225)*DEG2RAD) * (base_height / 2);
		shape[2].y = pos.y + sinf((rot-225)*DEG2RAD) * (base_height / 2);
	}

	// wrap spaceship between edges of room
	if(pos.x > WINDOW_WIDTH) pos.x = 0;
	if(pos.x < 0) pos.x = WINDOW_WIDTH;
	if(pos.y > WINDOW_HEIGHT) pos.y = 0;
	if(pos.y < 0) pos.y = WINDOW_HEIGHT;

	if(safe_mode) {
		safe_mode_count--;
		safe_mode = !(0 == safe_mode_count);
	}
};

void Player::draw() {
	Color c = COLOR_LINES;
	if(safe_mode) {
		float s = abs(sinf((float)GetTime()*7));
		c = ColorAlpha(WHITE, s);
	}
	DrawTriangleLines(*shape, *(shape+1), *(shape+2), c);
	DrawCircle((int)pos.x, (int)pos.y, 4, GREEN);
};

void Player::kill() {
	pos.x = WINDOW_WIDTH / 2;
	pos.y = WINDOW_HEIGHT / 2;
	vel = Vector2Zero();
	safe_mode = true;
	safe_mode_count = PLAYER_TIME_INVUL;
	lives--;
}

/* ==== Asteroid ==== */
enum AsteroidShape {
	Normal = 1,
	Tiny
};

struct Asteroid {
	bool active = false;
	Vector2 pos = { 0.0f, 0.0f };
	Vector2 vel = { 0.0f, 0.0f };
	float rot = 0.0f;
	float rot_vel = 0.0f;

	// draw shape
	Vector2* shape = nullptr;
	std::vector<PolarPoint> points;
	AsteroidShape size;

	void createShape(AsteroidShape shape_size);
	void deleteShape();
};

void Asteroid::createShape(AsteroidShape shape_size) {
	size = shape_size;
	int n = GetRandomValue(6, 10) + 2;
	float d = 360.0f / (n - 2);
	shape = new Vector2[n];

	// first point on center
	points.push_back(PolarPoint { 0.0f, 0.0f });
	shape[0] = Vector2Zero();

	// others points
	for(int i = 1; i < n-1; i++) {
		float r = d * (i - 1) * DEG2RAD;
		float l = RandRangeF(32.0f, 64.0f) / shape_size;
		points.push_back(PolarPoint { r, l });
		shape[i].x = l * cosf(r);
		shape[i].y = l * -sinf(r);
	}

	// last point close the fan
	points.push_back(PolarPoint { points[1].radians, points[1].distance });
	shape[n-1].x = shape[1].x;
	shape[n-1].y = shape[1].y;
}

void Asteroid::deleteShape() {
	points.clear();
	delete[] shape;
}

Asteroid* AsteroidRevive(Pool<Asteroid>* asteroid_pool, AsteroidShape shape_size, int x = 0, int y = 0) {
	Asteroid* asteroid = asteroid_pool->getObject();
	if(NULL == asteroid) return NULL;

	if(0 == x) x = Choose(GetRandomValue(-64, 128), GetRandomValue(WINDOW_WIDTH-128, WINDOW_WIDTH+64));
	if(0 == y) y = Choose(GetRandomValue(-64, 128), GetRandomValue(WINDOW_WIDTH-128, WINDOW_WIDTH+64));
	asteroid->pos.x = (float)x;
	asteroid->pos.y = (float)y;

	asteroid->deleteShape();
	asteroid->createShape(shape_size);

	// set vel
	asteroid->vel.x = RandRangeF(-1.0f, 1.0f);
	asteroid->vel.y = RandRangeF(-1.0f, 1.0f);
	asteroid->rot_vel = RandRangeF(-1.0f, 1.0f);

	return asteroid;
}

void AsteroidSpawnRandom(Pool<Asteroid>* asteroid_pool, int amount = 1) {
	int i = 0;
	do {
		AsteroidRevive(asteroid_pool, AsteroidShape::Normal);
	}while(++i < amount);
}

void AsteroidClearAll(Pool<Asteroid>* asteroid_pool) {
	for(auto &asteroid: asteroid_pool->objects) {
		asteroid.active = false;
		asteroid.points.clear();
		delete[] asteroid.shape;
		asteroid.shape = nullptr;
	}
	asteroid_pool->number_actives = 0;
}

int main(void) {

	InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Asteroid Game");
	SetTargetFPS(GetMonitorRefreshRate(0));

	// init and load sounds
	InitAudioDevice();
	Sound snd_laser_shoot = LoadSound("laserShoot.wav");
	Sound snd_explosion_player = LoadSound("explosionPlayer.wav");
	Sound snd_explosion_asteroid = LoadSound("explosionAsteroid.wav");
	SetSoundVolume(snd_laser_shoot, 0.05f);
	SetSoundVolume(snd_explosion_player, 0.05f);
	SetSoundVolume(snd_explosion_asteroid, 0.05f);

	SetRandomSeed(0xaabbccff);

	double time_shoot = GetTime();
	double time_asteroids_increment = GetTime();
	int game_state = 0; // 0 - press to player; 1 - in game; 2 - game over
	int asteroids_max = 8;
	
	Pool<Bullet> bullet_pool;
	Pool<Asteroid> asteroid_pool;
	Pool<Particle> particle_pool;

	Player* player = new Player();
	player->pos.x = WINDOW_WIDTH / 2;
	player->pos.y = WINDOW_HEIGHT / 2;
	int score = 0;

	// spawn asteroids
	AsteroidSpawnRandom(&asteroid_pool, ASTEROID_START_NUMBER);

	while(!WindowShouldClose()) {

		// in game
		if(1 == game_state) {

			// insance asteroids if is less than max number of asteroids
			if(asteroid_pool.number_actives < asteroids_max) {
				AsteroidSpawnRandom(&asteroid_pool);
			}

			// adds the maximum number of asteroids every 5 seconds
			if(GetTime() - time_asteroids_increment > 5 && asteroids_max < 20) {
				asteroids_max++;
				time_asteroids_increment = GetTime();
			}

			// update player
			if(IsKeyDown(KEY_W)) {
				player->vel.x += cosf(player->rot * DEG2RAD) * 0.1f;
				player->vel.y += sinf(player->rot * DEG2RAD) * 0.1f;
			}

			if(IsKeyDown(KEY_A)) {
				player->rot_vel -= 0.3f;
			}

			if(IsKeyDown(KEY_D)) {
				player->rot_vel += 0.3f;
			}
			
			player->update();

			// player shoot
			if(GetTime() - time_shoot > 0.1 && IsKeyDown(KEY_SPACE)) {
				Bullet* bullet = bullet_pool.getObject();
				if(bullet != NULL) {
					bullet->pos.x = player->pos.x;
					bullet->pos.y = player->pos.y;
					bullet->vel.x = cosf(player->rot * DEG2RAD) * player->bullet_speed_ratio;
					bullet->vel.y = sinf(player->rot * DEG2RAD) * player->bullet_speed_ratio;
					PlaySound(snd_laser_shoot);
				}
				time_shoot = GetTime();
			}
		}

		// update bullets
		for(auto &bullet: bullet_pool.objects) {
			if(!bullet.active) continue;

			bullet.pos = Vector2Add(bullet.pos, bullet.vel);
			
			// out of room
			if(!CheckCollisionPointRec(bullet.pos, Rectangle { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT })) {
				bullet_pool.kill(&bullet);
				continue;
			}

			for(auto &asteroid: asteroid_pool.objects) {
				if(!asteroid.active) continue;

				// bullet collision with asteroid
				if(CheckCollisionPointPoly(bullet.pos, asteroid.shape+1, (int)asteroid.points.size()-1)) {
					bullet_pool.kill(&bullet);

					score++;
					ParticleBurst(&particle_pool, asteroid.pos.x, asteroid.pos.y, 10);

					if(AsteroidShape::Normal == asteroid.size) {
						AsteroidRevive(&asteroid_pool, AsteroidShape::Tiny, (int)asteroid.pos.x, (int)asteroid.pos.y);
						AsteroidRevive(&asteroid_pool, AsteroidShape::Tiny, (int)asteroid.pos.x, (int)asteroid.pos.y);
					}
					asteroid_pool.kill(&asteroid);
					PlaySound(snd_explosion_asteroid);
					break;
				}
			}
		}

		// update asteroids
		for(auto &asteroid: asteroid_pool.objects) {
			if(!asteroid.active) continue;
			asteroid.pos = Vector2Add(asteroid.pos, asteroid.vel);
			asteroid.rot += asteroid.rot_vel;

			if(asteroid.pos.x < 0) asteroid.pos.x = WINDOW_WIDTH;
			if(asteroid.pos.x > WINDOW_WIDTH) asteroid.pos.x = 0;
			if(asteroid.pos.y < 0) asteroid.pos.y = WINDOW_HEIGHT;
			if(asteroid.pos.y > WINDOW_HEIGHT) asteroid.pos.y = 0;

			// update shape
			for(int i = 0; i < asteroid.points.size(); i++) {
				asteroid.points[i].radians += (1 * DEG2RAD);
				asteroid.shape[i].x = asteroid.pos.x + cosf(asteroid.points[i].radians) * asteroid.points[i].distance;
				asteroid.shape[i].y = asteroid.pos.y - sinf(asteroid.points[i].radians) * asteroid.points[i].distance;
				
				// player collision with asteroid
				if(!player->safe_mode && CheckCollisionPointTriangle(asteroid.shape[i], *(player->shape), *(player->shape+1), *(player->shape+2))) {
					ParticleBurst(&particle_pool, player->pos.x, player->pos.y, 5);
					player->kill();
					PlaySound(snd_explosion_player);
				}
			}
		}

		// game over
		if(0 == player->lives) {
			game_state = 2;
		}

		// update particles
		ParticleUpdate(&particle_pool);

	BeginDrawing();

		ClearBackground(COLOR_BACKGROUND);

		if(1 == game_state) {
			// draw player
			player->draw();

			// draw bullets 
			for(auto bullet: bullet_pool.objects) {
				if(!bullet.active) continue;
				DrawCircle((int)bullet.pos.x, (int)bullet.pos.y, 4, COLOR_BULLET);
			}
		}

		// draw asteroids
		for(auto asteroid: asteroid_pool.objects) {
			if(!asteroid.active) continue;
			DrawTriangleFan(asteroid.shape, (int)asteroid.points.size(), COLOR_BACKGROUND);
			DrawLineStrip(asteroid.shape+1, (int)asteroid.points.size()-1, COLOR_LINES);
		}

		// draw particles
		ParticleDraw(&particle_pool);

		// start game and draw start game menu
		if(0 == game_state) {
			if(IsKeyPressed(KEY_SPACE)) {
				game_state = 1;
				score = 0;
				AsteroidClearAll(&asteroid_pool);
				AsteroidSpawnRandom(&asteroid_pool, ASTEROID_START_NUMBER);
			}
			DrawText("ASTEROIDS", WINDOW_WIDTH / 2 - MeasureText("ASTEROIDS", 56) / 2, WINDOW_HEIGHT / 2 - 56, 56, COLOR_LINES);
			DrawText("PRESS SPACE TO PLAY", WINDOW_WIDTH / 2 - MeasureText("PRESS SPACE TO PLAY", 24) / 2, WINDOW_HEIGHT / 2 + 24, 24, COLOR_LINES);
		}

		// draw score in game
		if(1 == game_state) {
			const char* show_score = TextFormat("%04i", score);
			DrawText(show_score, WINDOW_WIDTH / 2 - MeasureText(show_score, 22) / 2 - 8, 8, 22, COLOR_LINES);
		}

		// show game over screen
		if(2 == game_state) {
			// reset game
			if(IsKeyPressed(KEY_SPACE)) {
				game_state = 0;
				player->pos.x = WINDOW_WIDTH / 2;
				player->pos.y = WINDOW_HEIGHT / 2;
				player->lives = 3;
				player->safe_mode = false;
				player->safe_mode_count = 0;
				asteroids_max = 8;
				AsteroidClearAll(&asteroid_pool);
				AsteroidSpawnRandom(&asteroid_pool, ASTEROID_START_NUMBER);
			}
			DrawText("GAME OVER", WINDOW_WIDTH / 2 - MeasureText("GAME OVER", 56) / 2, WINDOW_HEIGHT / 2 - 56, 56, COLOR_LINES);
			const char* show_score = TextFormat("SCORE %i", score);
			DrawText(show_score, WINDOW_WIDTH / 2 - MeasureText(show_score, 24) / 2, WINDOW_HEIGHT / 2 + 24, 24, COLOR_LINES);
		}

		/*
		DrawFPS(64, 0);
		DrawText(TextFormat("Player rot_vel: %02.02f", player->rot_vel), 8, 48, 12, RED);
		DrawText(TextFormat("Bullets: %02i", bullet_pool.number_actives), 8, 64, 12, RED);
		DrawText(TextFormat("Asteroids: %02i", asteroid_pool.number_actives), 8, 80, 12, RED);
		DrawText(TextFormat("Particles: %02i", particle_pool.number_actives), 8, 96, 12, RED);
		DrawText(TextFormat("Invul.: %i", player->safe_mode), 8, 112, 12, BLUE);
		DrawText(TextFormat("Invul. Frames: %i", player->safe_mode_count), 8, 128, 12, BLUE);
		DrawText(TextFormat("Lives: %i", player->lives), 8, 144, 12, BLUE);
		DrawText(TextFormat("Max Asteroids: %i", asteroids_max), 8, 160, 12, BLUE);
		*/

	EndDrawing();
	}

	// clear and close the game window
	delete player;
	AsteroidClearAll(&asteroid_pool);
	UnloadSound(snd_laser_shoot);

	CloseAudioDevice();
	CloseWindow();

	return 0;
}