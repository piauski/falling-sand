#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

#include <raylib.h>
#include <raymath.h>

#include "nob.h"

#define SCREEN_WIDTH  800
#define SCREEN_HEIGHT 600

#define CELL_SIZE_PX 10

#define FPS 120
#define INTERVAL 1

#define CLICK_RADIUS 10

#define GRID_WIDTH      (SCREEN_WIDTH/CELL_SIZE_PX)
#define GRID_HEIGHT     (SCREEN_HEIGHT/CELL_SIZE_PX)
#define GRID_INDEX(g, x, y) ((g)[GRID_WIDTH * x + y])

#define RAND_FLOAT ((float)(rand()) / (float)(RAND_MAX))

#define CLAMP(value, low, high) (((value) < (low)) ? (low) : (((value) > (high)) ? (high) : (value)))


typedef enum {
    PP_NONE           = 1 << 0,
    PP_MOVE_DOWN      = 1 << 1,
    PP_MOVE_DOWN_SIDE = 1 << 2,
    PP_MOVE_SIDE      = 1 << 3,
} Particle_Properties;

typedef enum {
    PT_EMPTY = 0,
    PT_SAND,
    PT_STONE,
    PT_WATER,
    PT_COUNT
} Particle_Type;

typedef struct {
    Particle_Type type;
    Particle_Properties props;
    Color color;
    bool updated;
} Particle;

void particle_set(Particle *p, Particle_Type type)
{
    p->props = PP_NONE;
    p->type = type;

    switch (type) {
        case PT_SAND: {
            p->props = PP_MOVE_DOWN | PP_MOVE_DOWN_SIDE;
            p->color = ColorBrightness(GOLD,((RAND_FLOAT*2)-1)/4);
            break;
        }
        case PT_STONE: {
            p->props = PP_NONE;
            p->color = ColorBrightness(GRAY,((RAND_FLOAT*2)-1)/4);
        }
        case PT_WATER: {
            p->props = PP_MOVE_DOWN | PP_MOVE_DOWN_SIDE | PP_MOVE_SIDE;
            p->color = ColorBrightness(BLUE,((RAND_FLOAT*2)-1)/4);
            break;
        }
        default: break;
    }
}

typedef struct Vector2i {
    int x;
    int y;
} Vector2i;

typedef struct {
    Vector2i *items;
    size_t count;
    size_t capacity;
} Particle_Updates;

typedef struct {
    size_t width;
    size_t height;
    double scale;
    Particle **particles;
    Particle_Updates particle_updates;

    Image image;
    Texture2D texture;
} World;

World *world_new(size_t width, size_t height, double scale)
{
    World *world = malloc(sizeof(World));
    assert(world && "Could not allocate world");
    memset(world, 0, sizeof(*world));

    world->width = width / scale;
    world->height = height / scale;
    world->scale = scale;

    Particle **particles = malloc(sizeof(Particle) * width * height);
    assert(particles && "Could not allocate particles");
    memset(particles, 0, sizeof(*particles));

    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            Particle *particle = malloc(sizeof(Particle));
            assert(particle && "Could not allocate Particle");
            memset(particle, 0, sizeof(Particle));
            particle->color = RED;
            particles[x + y * width] = particle;
        }
    }

    world->particles = particles;

    world->image = GenImageColor(world->width, world->height, RED);
    world->texture = LoadTextureFromImage(world->image);

    return world;
}

void world_free(World *world)
{
    for (size_t y = 0; y < world->height; ++y) {
        for (size_t x = 0; x < world->width; ++x) {
            free(world->particles[world->width * x + y]);
        }
    }
    UnloadImage(world->image);
    UnloadTexture(world->texture);
    nob_da_free(world->particle_updates);
    free(world);
}

Color *world_gen_image_data(World *world)
{
    Color *colors = malloc(sizeof(Color) * world->width * world->height);
    for (size_t i = 0; i < world->width * world->height; ++i) {
        colors[i] = world->particles[i]->color;
    }
    return colors;
}

Color *world_update_image_data(World *world, Color *colors)
{
    for (size_t i = 0; i < world->width * world->height; ++i) {
        colors[i] = world->particles[i]->color;
    }
    return colors;
}

size_t world_get_index(World *world, size_t x, size_t y) {
    return x + y * world->width;
}

Particle *world_get_at_index(World *world, size_t i) {
    return world->particles[i];
}

Particle *world_get_at(World *world, size_t x, size_t y) {
    return world_get_at_index(world, world_get_index(world, x, y));
}

bool world_in_bounds(World *world, size_t x, size_t y) {
    return x < world->width && y < world->height;
}

bool world_is_empty(World *world, size_t x, size_t y) {
    return world_in_bounds(world, x, y) && world_get_at(world, x, y)->type == PT_EMPTY;
}


void world_set_particle(World *world, size_t x, size_t y, Particle* p)
{
    world->particles[world_get_index(world, x, y)] = p;
}


void world_move_particle(World *world, size_t x_src, size_t y_src, size_t x_dst, size_t y_dst) {

    Vector2i update = {
        .x = world_get_index(world, x_dst, y_dst),
        .y = world_get_index(world, x_src, y_src)
    };
    nob_da_append(&world->particle_updates, update);
    // Particle *p_dst = world_get_at(world, x_dst, y_dst);
    // Particle *p_src = world_get_at(world, x_src, y_src);

    // if (p_dst->type == PT_EMPTY) {
    //     world_set_particle(world, x_src, y_src, p_dst);
    //     world_set_particle(world, x_dst, y_dst, p_src);
    // }
}

void world_update_particles(World *world)
{
    if (world->particle_updates.count < 1) return;

    // remove moves that have their dst filled
    for (size_t i = 0; i < world->particle_updates.count; ++i) {
        Vector2i *it = &world->particle_updates.items[i]; // { .x = dst, .y = src }
        if (world->particles[it->x]->type != PT_EMPTY) {
            it->x = -1;
            it->y = -1;
        }
    }

    // shuffle the array using the Fisher-Yates algorithm
    nob_da_append(&world->particle_updates, (Vector2i){ 0 });
    for (size_t i = 0; i < world->particle_updates.count - 1; ++i) {
        size_t j = i + rand() / (RAND_MAX / (world->particle_updates.count - i) + 1);
        Vector2i temp = world->particle_updates.items[j];
        world->particle_updates.items[j] = world->particle_updates.items[i];
        world->particle_updates.items[i] = temp;
    }

    for (size_t i = 0; i < world->particle_updates.count; ++i) {
        Vector2i it = world->particle_updates.items[i];
        // swap the cells if both src and dst exist.
        if (it.x >= 0 && it.y >= 0) {
            Particle *p_dst = world_get_at_index(world, it.x);
            Particle *p_src = world_get_at_index(world, it.y);

            world->particles[it.x] = p_src;
            world->particles[it.y] = p_dst;
        }
    }

    world->particle_updates.count = 0;
}

bool world_move_down(World *world, size_t x, size_t y)
{
    bool down = world_is_empty(world, x, y + 1);
    if (down)
    world_move_particle(world, x, y, x, y + 1);
    return down;
}

bool world_move_down_side(World *world, size_t x, size_t y)
{
    bool down_left = world_is_empty(world, x - 1, y + 1);
    bool down_right = world_is_empty(world, x + 1, y + 1);

    if (down_left && down_right) {
        down_left = GetRandomValue(0,1) == 0;
        down_right = !down_left;
    }

    if (down_left)
        world_move_particle(world, x, y, x - 1, y + 1);
    else if (down_right)
        world_move_particle(world, x, y, x + 1, y + 1);

    return down_left || down_right;
}

bool world_move_side(World *world, size_t x, size_t y)
{
    bool left = world_is_empty(world, x - 1, y);
    bool right = world_is_empty(world, x + 1, y);

    if (left && right) {
        left = GetRandomValue(0, 1) == 0;
        right = !left;
    }

    if (left)
        world_move_particle(world, x, y, x - 1, y);
    else if (right)
        world_move_particle(world, x, y, x + 1, y);

    return left || right;
}

/*void particle_set(Particle **board, size_t x, size_t y, Material_Id mat)
{
    Particle *p = board_get(board,x,y);
    p->id = mat;
    p->color = material_get_color(mat);
}*/

int main()
{
    InitWindow(800, 600, "Falling Sand");

    SetTargetFPS(0);

    float accumulator = 0.0f;
    float current_time = GetTime();
    const float fixed_update_time = 1.0f / 62.0f;

    Vector2 mouse_pos;
    int hovered_grid_x = 0;
    int hovered_grid_y = 0;
    int interval = 0;

    size_t selected = PT_SAND;

    World *world = world_new(SCREEN_WIDTH, SCREEN_HEIGHT, 4);

    // for (size_t y = 100; y < 120; ++y) {
    //     for (size_t x = 50; x < world->width - 50; ++x) {
    //         Particle *p = world_get_at(world, x, y);
    //         p->type = PT_SAND;
    //         p->props = PP_MOVE_DOWN & PP_MOVE_DOWN_SIDE;
    //         p->color = particle_get_color(PT_SAND);
    //     }
    // }


    Color *world_image_data = world_gen_image_data(world);

    while (!WindowShouldClose()) {
        mouse_pos = Vector2Scale(GetMousePosition(), (float)1/(float)world->scale);
        //hovered_grid_x = (int)mouse_pos.x/CELL_SIZE_PX;
        //hovered_grid_y = (int)mouse_pos.y/CELL_SIZE_PX;

        //interval++;
        //if (interval >= INTERVAL) {
            //    interval = 0;
            /*for (size_t y = GRID_HEIGHT - 1; y > 0; --y) {
                for (size_t x = 0; x < GRID_WIDTH; ++x) {
                    Particle* p = board_get(board, x, y);
                    assert(Material_Id_Count == 4 && "ERROR: Material update switch case not exhaustive");
                    switch(p->id) {
                        case Material_Id_Empty:
                        break;
                        case Material_Id_Sand:
                        board_update_sand(board, x, y);
                        case Material_Id_Stone:
                        break;
                        case Material_Id_Water:
                        board_update_water(board, x, y);
                        case Material_Id_Count:
                        break;
                    }
                }
            }*/
            //}
            float new_time = GetTime();
            float frame_time = new_time - current_time;
            current_time = new_time;
            accumulator += frame_time;
            while (accumulator >= fixed_update_time)
            {
                //update particles

                for (size_t y = world->height - 1; y > 0; --y) {
                    for (size_t x = 0; x < world->width; ++x) {
                        Particle *p = world_get_at(world, x, y);

                        if ((p->props & PP_MOVE_DOWN) && world_move_down(world, x, y)) {}
                        else if ((p->props & PP_MOVE_DOWN_SIDE) && world_move_down_side(world, x, y)) {}
                        else if ((p->props & PP_MOVE_SIDE) && world_move_side(world, x, y)) {}

                    }
                }
                // TODO: use the Fisher-Yates shuffle algorithm to randomly
                // shuffle the array then loop over it. i += 2 to skip over
                // each pair, then iterate over them and commit the changes
                // as such. Allocate a new array twice the size as particles
                // then free in world_update_particles.
                world_update_particles(world);

                //input
                if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                    for (int i = -CLICK_RADIUS; i < CLICK_RADIUS; ++i) {
                        for (int j = -CLICK_RADIUS; j < CLICK_RADIUS; ++j) {
                            Vector2 offset = { .x = i, .y = j };
                            Vector2 hovered = { .x = mouse_pos.x, .y = mouse_pos.y };
                            size_t new_x = (size_t)mouse_pos.x + i;
                            size_t new_y = (size_t)mouse_pos.y + j;
                            if (Vector2DistanceSqr(hovered, Vector2Add(hovered, offset)) <= CLICK_RADIUS*CLICK_RADIUS &&
                            GetRandomValue(1,10) == 1 &&
                            world_is_empty(world, new_x, new_y))
                            {
                                particle_set(world_get_at(world, new_x, new_y), selected);
                                //world_set_particle(world, hovered_grid_x + i, hovered_grid_y + j, selected);
                            }
                        }
                    }

                }
                if (IsKeyDown(KEY_ONE)) selected = PT_SAND;
                if (IsKeyDown(KEY_TWO)) selected = PT_STONE;
                if (IsKeyDown(KEY_THREE)) selected = PT_WATER;
                accumulator -= fixed_update_time;
            }
            BeginDrawing();
            //ClearBackground(RED);

            world_image_data = world_update_image_data(world, world_image_data);

            UpdateTexture(world->texture, world_image_data);



            DrawTexturePro(
            world->texture,
            (Rectangle){
                .width = world->width,
                .height = world->height
            },
            (Rectangle){
                .width = world->width * world->scale,
                .height = world->height * world->scale,
            },
            (Vector2){0, 0},
            0.0f,
            WHITE
            );
            //board_draw(board);
            DrawText(TextFormat("x: %.f, y: %.f", mouse_pos.x, mouse_pos.y), 0, 0, 25, WHITE);
            DrawText(TextFormat("Material: %d",selected), 0, 25, 25, WHITE);
            const char* fps_text = TextFormat("FPS: %d",GetFPS());
            DrawText(fps_text, SCREEN_WIDTH-MeasureText(fps_text,25),0,25,WHITE);
            //DrawCircleV(mouse_pos, 10, GRAY);
            EndDrawing();
        }

        free(world_image_data);
        world_free(world);

        CloseWindow();
        return 0;
    }
