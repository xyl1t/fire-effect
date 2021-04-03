#include <SDL.h>
#include <cmath>
#include <set>
#include <utility>
#include "cidr.hpp"

using namespace Cidr;

int main(int argc, char** argv) {
	SDL_Init(SDL_INIT_EVERYTHING);
	
	const float ZOOM = (argc > 1) ? std::atoi(argv[1]) : 6;

	const int CANVAS_WIDTH = 800/ZOOM;
	const int CANVAS_HEIGHT = 600/ZOOM;
	const int WINDOW_WIDTH = 800;
	const int WINDOW_HEIGHT = 600;
	
	SDL_Window* window = SDL_CreateWindow("Fire Effect", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_ALLOW_HIGHDPI);
	SDL_Renderer* renderer = SDL_CreateRenderer(window, 0, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR32, SDL_TEXTUREACCESS_STREAMING, CANVAS_WIDTH, CANVAS_HEIGHT);
	SDL_ShowCursor(SDL_DISABLE);
	
	uint32_t* fire = new uint32_t[CANVAS_WIDTH * CANVAS_HEIGHT] {0};
	std::set<std::pair<int,int>> fireballs;
	
	Bitmap img(CANVAS_WIDTH, CANVAS_HEIGHT);
	
	Cidr::Renderer rend {img.GetData(), CANVAS_WIDTH, CANVAS_HEIGHT};
	
	int mx{};
	int my{};
	
	// create palettes
	RGBA palette256[256]{};
	for (int i = 0; i < 256; i++) {
		HSL p;
		p.setH(12 + i/5.3f);
		p.setS(1);
		p.setL(i/240.f);
		
		palette256[i] = HSLtoRGB(p);
	}
	RGBA palette16[16]{};
	for (int i = 0; i < 16; i++) {
		HSL p;
		p.setH(12 + i*3);
		p.setS(1);
		p.setL(i/16.f);
		
		palette16[i] = HSLtoRGB(p);
	}
	RGBA palette8[8]{};
	for (int i = 0; i < 8; i++) {
		HSL p;
		p.setH(13 + i*6);
		p.setS(1);
		p.setL(i/7.5f);
		
		palette8[i] = HSLtoRGB(p);
	}
	RGBA palette4[4]{};
	for (int i = 0; i < 4; i++) {
		HSL p;
		p.setH(13 + i*12);
		p.setS(1);
		p.setL(i/4.f);
		
		palette4[i] = HSLtoRGB(p);
	}
	
	constexpr int size = 8;	
	int matrix[size][size] {
		{ 0, 48, 12, 60, 3, 51, 15, 63 },
		{ 32, 16, 44, 28, 35, 19, 47, 31 },
		{ 8, 56, 4, 52, 11, 59, 7, 55 },
		{ 40, 24, 36, 20, 43, 27, 39, 23 },
		{ 2, 50, 14, 62, 1, 49, 13, 61 },
		{ 34, 18, 46, 30, 33, 17, 45, 29 },
		{ 10, 58, 6, 54, 9, 57, 5, 53 },
		{ 42, 26, 38, 22, 41, 25, 37, 21 },
	};
	
	// constexpr int size = 4;
	// int matrix[size][size] {
	// 	{ 0, 8, 2, 10 },
	// 	{ 12, 4, 14, 6 },
	// 	{ 3, 11, 1, 9 },
	// 	{ 15, 7, 13, 5 },
	// };
	
	float thresholdMap[size][size];
	for (int i = 0; i < size; i ++) {
		for (int j = 0; j < size; j++) {
			thresholdMap[i][j] = (matrix[i][j] + 1) / (float)(size * size) - 0.5;
		}
	}
	
	auto getClosesColor = [](RGBA* palette, int size, const RGBA& input) {
		int closest{0};
		float inputVal = (input.r*input.r + input.g*input.g + input.b*input.b);
		float diff = std::abs((palette[0].r*palette[0].r + palette[0].g*palette[0].g + palette[0].b*palette[0].b) - inputVal);
		for (int i = 1; i < size; i++) {
			float val = (palette[i].r*palette[i].r + palette[i].g*palette[i].g + palette[i].b*palette[i].b);
			
			if(std::abs(val - inputVal) < diff) {
				diff = std::abs(val - inputVal);
				closest = i;
			}
		}
		return palette[closest];
	};
	
	int fireballRadius = 2;
	bool animate = false;
	
	SDL_Event e;
	bool alive = true;
	while (alive) {
		while (SDL_PollEvent(&e)) {
			switch(e.type) {
				case SDL_QUIT: 
					alive = false;
					break;
				case SDL_KEYDOWN:
					if (e.key.keysym.sym == SDLK_SPACE) {
						animate = !animate;
					}
					break;
			}
		}
		SDL_GetMouseState(&mx, &my);
		
		// Checks whether the cursor is outside the window. If so, it puts the cursor in the middle of the window
		int wx = 0, wy = 0;
		SDL_GetWindowPosition(window, &wx, &wy);
		int gmx = 0, gmy = 0;
		SDL_GetGlobalMouseState(&gmx, &gmy);
		if(gmx < wx || gmy < wy || gmx >= wx + WINDOW_WIDTH || gmy >= wy + WINDOW_HEIGHT) {
			mx = WINDOW_WIDTH / 2;
			my = WINDOW_HEIGHT / 2;
		}
		
		mx /= ZOOM;
		my /= ZOOM;
		mx = std::clamp(mx, 0, CANVAS_WIDTH);
		my = std::clamp(my, 0, CANVAS_WIDTH);

		
		rend.Clear();
		
		auto setFire = [&](int x, int y, int value) {
			fire[(x+CANVAS_WIDTH) % CANVAS_WIDTH + (y+CANVAS_HEIGHT) % CANVAS_HEIGHT * CANVAS_WIDTH] = std::clamp(value, 0, 255);
		};
		auto getFire = [&](int x, int y) {
			return fire[(x+CANVAS_WIDTH) % CANVAS_WIDTH + (y+CANVAS_HEIGHT) % CANVAS_HEIGHT * CANVAS_WIDTH];
		};
		
		// draw fireball on window if user holds left mouse button
		if (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT)) {
			for (int i = -fireballRadius; i < fireballRadius; i++) {
				for (int j = -fireballRadius; j < fireballRadius; j++) {
					if(i * i + j * j < fireballRadius * fireballRadius) {
						fireballs.insert(std::make_pair(mx + i, my + j));
					}
				}
			}
		}
		
		// erase fireball from window if user holds right mouse button
		if (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_RIGHT)) {
			for (int i = -fireballRadius; i < fireballRadius; i++) {
				for (int j = -fireballRadius; j < fireballRadius; j++) {
					if(i * i + j * j < fireballRadius * fireballRadius) {
						fireballs.erase(std::make_pair(mx + i, my + j));
					}
				}
			}

		}
		
		// put fireballs on fire buffer
		for (const auto& fireball : fireballs) {
			setFire(fireball.first, fireball.second, 128+rand() % 127);
		}
		
		// Put fire on the bottom of the window
		for (int x = 0; x < CANVAS_WIDTH; x++) {
			setFire(x, CANVAS_HEIGHT - 1, rand() % 256);
		}
		
		// Fireball following the cursor
		for (int i = -fireballRadius; i < fireballRadius; i++) {
			for (int j = -fireballRadius; j < fireballRadius; j++) {
				if(i * i + j * j < fireballRadius * fireballRadius) {
					setFire(mx + i, my + j, 128+rand() % 127);
				}
			}
		}
		
		// Fire X
		// int xsize = 24;
		// for (int x = 0; x < xsize; x++) {
		// 	for (int y = 0; y < 4; y++) {
		// 		setFire(x + CANVAS_WIDTH/4-xsize/2, x + CANVAS_HEIGHT/2 + y-xsize/2, 255);
		// 		setFire(-x + CANVAS_WIDTH/4+xsize/2, x + CANVAS_HEIGHT/2 + y-xsize/2, 255);
				
		// 		setFire(x + 3*CANVAS_WIDTH/4-xsize/2, x + CANVAS_HEIGHT/2 + y-xsize/2, 255);
		// 		setFire(-x + 3*CANVAS_WIDTH/4+xsize/2, x + CANVAS_HEIGHT/2 + y-xsize/2, 255);
				
		// 		if(animate && !(rand()%8)) {
		// 			setFire(x + CANVAS_WIDTH/4-xsize/2 , x + CANVAS_HEIGHT/2 + y-xsize/2, rand()%256);
		// 			setFire(-x + CANVAS_WIDTH/4+xsize/2, x + CANVAS_HEIGHT/2 + y-xsize/2, rand()%256);
					
		// 			setFire(x + 3*CANVAS_WIDTH/4-xsize/2, x + CANVAS_HEIGHT/2 + y-xsize/2, rand()%256);
		// 			setFire(-x + 3*CANVAS_WIDTH/4+xsize/2, x + CANVAS_HEIGHT/2 + y-xsize/2, rand()%256);
		// 		}
		// 	}
		// }
		
		// Fire effect
		for (int y = 0; y < CANVAS_HEIGHT - 1; y++) {
			for (int x = 0; x < CANVAS_WIDTH; x++) {
				setFire(x, y, 
					(getFire(x, y+1) + 
					getFire(x+1, y+1) + 
					getFire(x-1, y+1) + 
					getFire(x, y+2)) / 5.0f);
			}
		}
		
		// Draw first half of window
		for (int y = 0; y < CANVAS_HEIGHT; y++) {
			for (int x = 0; x < CANVAS_WIDTH/2; x++) {
				rend.DrawPoint(palette256[getFire(x,y)], x, y);
			}
		}
		
		// Draw second half of the window using ordered dithering 
		for (int y = 0; y < CANVAS_HEIGHT; y++) {
			for (int x = CANVAS_WIDTH/2; x < CANVAS_WIDTH; x++) {
				RGBA p = palette256[getFire(x,y)];
				float factor = thresholdMap[x % size][ y % size];
				
				float rdepth = 4;
				float gdepth = 4;
				float bdepth = 4;
				
				int r = p.r + (255.f / rdepth) * (thresholdMap[x % size][y % size]);
				int g = p.g + (255.f / gdepth) * (thresholdMap[x % size][y % size]);
				int b = p.b + (255.f / bdepth) * (thresholdMap[x % size][y % size]);

				r = std::clamp(r, 0, 255);
				g = std::clamp(g, 0, 255);
				b = std::clamp(b, 0, 255);
				
				RGBA attempt{};
				
				attempt.r = std::round(rdepth * r / 255.f) * (255.f / rdepth);
				attempt.g = std::round(gdepth * g / 255.f) * (255.f / gdepth);
				attempt.b = std::round(bdepth * b / 255.f) * (255.f / bdepth);
				
				RGBA c = getClosesColor(palette16, 16, attempt);
				// RGBA c = attempt;
				rend.DrawPoint(c, x, y);
			}
		}
		
		// Draw palette
		// for (int i = 0; i < 48; i++) {
		// 	for (int j = 0; j < 4; j++) {
		// 		rend.DrawPoint(palette256[i*(255/48)], i, j);
		// 		rend.DrawPoint(palette16[(int)(i*(16/48.f))], i, j+4);
		// 	}
		// }
		
		// Display image
		SDL_UpdateTexture(texture, nullptr, img.GetData(), CANVAS_WIDTH * sizeof(uint32_t));
		SDL_RenderClear(renderer);
		SDL_Rect rect1{0,0, CANVAS_WIDTH, CANVAS_HEIGHT};
		SDL_Rect rect2{0,0, WINDOW_WIDTH, WINDOW_HEIGHT};
		SDL_RenderCopy(renderer, texture, &rect1, &rect2);
		SDL_RenderPresent(renderer);
	}
	
	// Save image
	img.SaveAs("fireEffect.png", BaseBitmap::Formats::PNG);
	
	SDL_Quit();
	
	return 0;
}