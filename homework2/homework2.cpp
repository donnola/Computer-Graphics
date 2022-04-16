// Include standard headers
#include <stdio.h>
#include <stdlib.h>

// Include GLEW
#include <GL/glew.h>

// Include GLFW
#include <GLFW/glfw3.h>
GLFWwindow* window;

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
using namespace glm;

#include <common/shader.hpp>
#include <common/controls.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <vector>
#include <ctime>
#include <iostream>
#include <common/objloader.hpp>
#include <common/texture.hpp>

struct Object{
    vec3 pos;
    bool life;
    float collider_rad;
};

// CPU representation of a particle
struct Enemy : Object{
    vec3 pos;
    bool life = true;
    float collider_rad = 1.0f;
    vec4 quaternion;
    float dist;

    bool operator<(const Enemy& that) const {
        // Sort in reverse order : far particles drawn first.
        return this->dist > that.dist;
    }

    Enemy(vec3 p, vec3 axis, float angle) : pos(p){
        float half_angle = (angle * 0.5) * 3.14159 / 180.0;
        quaternion.x = axis.x * sin(half_angle);
        quaternion.y = axis.y * sin(half_angle);
        quaternion.z = axis.z * sin(half_angle);
        quaternion.w = cos(half_angle);
        vec3 cam_pos = getCameraPosition();
        dist = distance(pos, cam_pos);
    }
};

struct Projectile : Object{
    vec3 pos;
    bool life = true;
    float collider_rad = 0.25f * 2;
    vec3 direction;
    float speed = 15.0f;

    explicit Projectile(vec3 p, vec3 dir) : pos(p), direction(dir) {}
};

const int MaxEnemies = 20;
const int MaxProjectiles = 50;
std::vector<Enemy> enemyContainer;
GLint KilledEnemyCount = 0;
std::vector<Projectile> projectileContainer;

void MoveProjectiles(float delta_time){
    for (Projectile& proj : projectileContainer){
        proj.pos += proj.direction * proj.speed * (float)delta_time;
    }
}

void CheckCollision(){
    for (Projectile& proj : projectileContainer){
        if(!proj.life){
            continue;
        }
        for (Enemy& enemy : enemyContainer){
            if(!enemy.life){
                continue;
            }
            float dist = distance(enemy.pos, proj.pos);
            if (dist <= enemy.collider_rad + proj.collider_rad) {
                KilledEnemyCount += 1;
                enemy.life = false;
                proj.life = false;
            }
        }
    }
}

template <typename T>
void DeleteDestroyedObject(std::vector<T>& objects){
    std::vector<int> deleted_ind;
    for (int i = objects.size() - 1; i >= 0; --i) {
        if (!objects[i].life) {
            objects.erase(objects.begin() + i);
        }
    }
}

void DeleteFarProjectiles(){
    for (int i = (int)projectileContainer.size() - 1; i >= 0; --i) {
        vec3 cam_pos = getCameraPosition();
        float dist = distance(projectileContainer[i].pos, cam_pos);
        if (dist >= 35) {
            projectileContainer.erase(projectileContainer.begin() + i);
        }
    }
}

void SortEnemies(){
    std::sort(enemyContainer.begin(), enemyContainer.end());
}

void CreateEnemy(){
    float w = rand() % 360;
    float x_q = rand() % 21 - 10;
    float y_q = rand() % 21 - 10;
    float z_q = rand() % 21 - 10;

    float x_p = rand() % 21 - 10;
    float y_p = rand() % 21 - 10;
    float z_p = rand() % 21 - 10;
    float rad = rand() % 20 + 12;

    vec3 pos(x_p, y_p, z_p);
    pos += getCameraPosition();
    vec3 rot_axis(x_q, y_q, z_q);
    pos = normalize(pos) * rad;
    enemyContainer.emplace_back(Enemy(pos, normalize(rot_axis), w));
}

int main( void )
{
	// Initialise GLFW
	if( !glfwInit() )
	{
		fprintf( stderr, "Failed to initialize GLFW\n" );
		getchar();
		return -1;
	}

	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); //We don't want the old OpenGL 

	// Open a window and create its OpenGL context
	window = glfwCreateWindow( 1024, 768, "Homework 2 - Shooter", nullptr, nullptr);
	if( window == nullptr ){
		fprintf( stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Try the 2.1 version of the tutorials.\n" );
		getchar();
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	// Initialize GLEW
	glewExperimental = true; // Needed for core profile
	if (glewInit() != GLEW_OK) {
		fprintf(stderr, "Failed to initialize GLEW\n");
		getchar();
		glfwTerminate();
		return -1;
	}

	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);

    glfwPollEvents();
	// Dark blue background
	glClearColor(0.0f, 0.0f, 0.4f, 0.0f);

    // Enable depth test
    glEnable(GL_DEPTH_TEST);
//    // Отсечение тех треугольников, нормаль которых направлена от камеры
//    glEnable(GL_CULL_FACE);
    // Accept fragment if it closer to the camera than the former one
    glDepthFunc(GL_LESS);

	GLuint VertexArrayID;
	glGenVertexArrays(1, &VertexArrayID);
	glBindVertexArray(VertexArrayID);

	// Create and compile our GLSL program from the shaders
    GLuint programID1 = LoadShaders( "Enemy.vertexshader", "Enemy.fragmentshader" );
    GLuint programID2 = LoadShaders( "Projectile.vertexshader", "Projectile.fragmentshader" );

	// Get a handle for our "MVP" uniform
	GLuint MatrixID1 = glGetUniformLocation(programID1, "MVP");
	GLuint MatrixID2 = glGetUniformLocation(programID2, "MVP");

    GLuint TextureID  = glGetUniformLocation(programID2, "ProjectileTexture");

    GLuint Texture = loadDDS("uvmap.dds");


    // Read our .obj file
    std::vector<vec3> vertices_proj;
    std::vector<vec2> uvs_proj;
    std::vector<vec3> normals_proj; // Won't be used at the moment.
    bool res1 = loadOBJ("sphera_v04.obj", vertices_proj, uvs_proj, normals_proj);

    static std::vector<vec3> g_enemy_position_data(MaxEnemies);
    static std::vector<vec4> g_enemy_quat_data(MaxEnemies);
    static std::vector<vec3> g_projectile_position_data(MaxProjectiles);

    static const GLfloat g_vertex_buffer_data[] = {
            0.0f, 1.0f, 0.0f,
            -1.0f, 0.0f, -1.0f,
            -1.0f, 0.0f, 1.0f,

            0.0f, 1.0f, 0.0f,
            -1.0f, 0.0f, -1.0f,
            1.0f, 0.0f, -1.0f,

            0.0f, 1.0f, 0.0f,
            1.0f, 0.0f, 1.0f,
            1.0f, 0.0f, -1.0f,

            0.0f, 1.0f, 0.0f,
            1.0f, 0.0f, 1.0f,
            -1.0f, 0.0f, 1.0f,

            0.0f, -1.0f, 0.0f,
            -1.0f, 0.0f, -1.0f,
            -1.0f, 0.0f, 1.0f,

            0.0f, -1.0f, 0.0f,
            -1.0f, 0.0f, -1.0f,
            1.0f, 0.0f, -1.0f,

            0.0f,- 1.0f, 0.0f,
            1.0f, 0.0f, 1.0f,
            1.0f, 0.0f, -1.0f,

            0.0f, -1.0f, 0.0f,
            1.0f, 0.0f, 1.0f,
            -1.0f, 0.0f, 1.0f
    };

    // One color for each vertex. They were generated randomly.
    static const GLfloat g_color_buffer_data[] = {
            0.8f, 0.6f, 1.0f,
            0.3f, 0.0f, 0.1f,
            0.05f, 0.1f, 0.6f,

            0.8f, 0.6f, 1.0f,
            0.3f, 0.0f, 0.1f,
            0.05f, 0.1f, 0.6f,

            0.8f, 0.6f, 1.0f,
            0.3f, 0.0f, 0.1f,
            0.05f, 0.1f, 0.6f,

            0.8f, 0.6f, 1.0f,
            0.3f, 0.0f, 0.1f,
            0.05f, 0.1f, 0.6f,

            0.08f, 0.6f, 0.85f,
            0.3f, 0.0f, 0.1f,
            0.05f, 0.1f, 0.6f,

            0.08f, 0.6f, 0.85f,
            0.3f, 0.0f, 0.1f,
            0.05f, 0.1f, 0.6f,

            0.08f, 0.6f, 0.85f,
            0.3f, 0.0f, 0.1f,
            0.05f, 0.1f, 0.6f,

            0.08f, 0.6f, 0.85f,
            0.3f, 0.0f, 0.1f,
            0.05f, 0.1f, 0.6f
    };

    // points of the enemy
	GLuint enemy_vertex_buffer;
	glGenBuffers(1, &enemy_vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, enemy_vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data), g_vertex_buffer_data, GL_STATIC_DRAW);

    // positions of the enemy
    GLuint enemy_position_buffer;
    glGenBuffers(1, &enemy_position_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, enemy_position_buffer);
    glBufferData(GL_ARRAY_BUFFER, MaxEnemies * sizeof(vec3), nullptr, GL_STREAM_DRAW);

    // quaternion of the enemy
    GLuint enemy_rotation_axis_buffer;
    glGenBuffers(1, &enemy_rotation_axis_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, enemy_rotation_axis_buffer);
    glBufferData(GL_ARRAY_BUFFER, MaxEnemies * sizeof(vec4), nullptr, GL_STREAM_DRAW);

    // color of the enemy
    GLuint enemy_color_buffer;
    glGenBuffers(1, &enemy_color_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, enemy_color_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(g_color_buffer_data), g_color_buffer_data, GL_STREAM_DRAW);

    // points of the projectile
    GLuint projectile_vertex_buffer;
    glGenBuffers(1, &projectile_vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, projectile_vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, vertices_proj.size() * sizeof(vec3), &vertices_proj[0], GL_STREAM_DRAW);

    // positions of the projectile
    GLuint projectile_position_buffer;
    glGenBuffers(1, &projectile_position_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, projectile_position_buffer);
    glBufferData(GL_ARRAY_BUFFER, MaxProjectiles * sizeof(vec3), nullptr, GL_STREAM_DRAW);

    // uvs of the projectile
    GLuint projectile_uvbuffer;
    glGenBuffers(1, &projectile_uvbuffer);
    glBindBuffer(GL_ARRAY_BUFFER, projectile_uvbuffer);
    glBufferData(GL_ARRAY_BUFFER, uvs_proj.size() * sizeof(vec2), &uvs_proj[0], GL_STATIC_DRAW);

    double last_time = glfwGetTime();
    double create_time = 0.0f;
    bool mouse_left_pressed = false;
    bool mouse_left_released = true;
    srand(time(0));
	do{
        // Вычислить MVP-матрицу в зависимости от положения мыши и нажатых клавиш
        computeMatricesFromInputs();
        glm::mat4 ProjectionMatrix = getProjectionMatrix();
        glm::mat4 ViewMatrix = getViewMatrix();
        glm::mat4 ModelMatrix = glm::mat4(1.0);
        glm::mat4 MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;

        double current_time = glfwGetTime();
        double delta = current_time - last_time;
        last_time = current_time;

        MoveProjectiles(float(delta));
        // creating enemies
        create_time -= delta;
        if (create_time <= 0 && enemyContainer.size() < MaxEnemies){
            CreateEnemy();
            create_time = 3.0f;
            SortEnemies();
        }

        // одинарное нажатие мыши
        if (mouse_left_released && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS && projectileContainer.size() < MaxProjectiles) {
            mouse_left_pressed = true;
            mouse_left_released = false;
        }

        if (mouse_left_pressed && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE) {
            mouse_left_pressed = false;
            mouse_left_released = true;
            std::cout << "create projectile\n";
            vec3 camera_pos = getCameraPosition();
            vec3 camera_direction = getCameraDirection();
            projectileContainer.emplace_back(Projectile(camera_pos + normalize(camera_direction), normalize(camera_direction)));
        }


        for (int i = 0; i < enemyContainer.size(); ++i){
            Enemy& enemy = enemyContainer[i];
            g_enemy_position_data[i] = enemy.pos;
            g_enemy_quat_data[i] = enemy.quaternion;
        }

        for (int i = 0; i < projectileContainer.size(); ++i){
            Projectile& proj = projectileContainer[i];
            g_projectile_position_data[i] = proj.pos;
        }

        glBindBuffer(GL_ARRAY_BUFFER, enemy_position_buffer);
        glBufferData(GL_ARRAY_BUFFER, MaxEnemies * sizeof(vec3), nullptr, GL_STREAM_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, enemyContainer.size() * sizeof(vec3), &g_enemy_position_data[0]);

        glBindBuffer(GL_ARRAY_BUFFER, enemy_rotation_axis_buffer);
        glBufferData(GL_ARRAY_BUFFER, MaxEnemies * sizeof(quat), nullptr, GL_STREAM_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, enemyContainer.size() * sizeof(vec4) * 4, &g_enemy_quat_data[0]);

        glBindBuffer(GL_ARRAY_BUFFER, projectile_position_buffer);
        glBufferData(GL_ARRAY_BUFFER, MaxProjectiles * sizeof(vec3), nullptr, GL_STREAM_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, projectileContainer.size() * sizeof(vec3), &g_projectile_position_data[0]);


		// Clear the screen
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Enemies
		glUseProgram(programID1);

		// Send our transformation to the currently bound shader, 
		// in the "MVP" uniform
		glUniformMatrix4fv(MatrixID1, 1, GL_FALSE, &MVP[0][0]);

        // 1 attribute buffer : enemy_vertex_buffer
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, enemy_vertex_buffer);
        glVertexAttribPointer(
                0,                  // attribute
                3,                  // size
                GL_FLOAT,           // type
                GL_FALSE,           // normalized?
                0,                  // stride
                (void*)0            // array buffer offset
        );

        // 2 attribute buffer : enemy_rotation_axis_buffer
        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, enemy_rotation_axis_buffer);
        glVertexAttribPointer(
                1,                  // attribute
                4,                  // size
                GL_FLOAT,           // type
                GL_FALSE,           // normalized?
                0,                  // stride
                (void*)0            // array buffer offset
        );

        // 3 attribute buffer : enemy_position_buffer
        glEnableVertexAttribArray(2);
        glBindBuffer(GL_ARRAY_BUFFER, enemy_position_buffer);
        glVertexAttribPointer(
                2,                  // attribute
                3,                  // size
                GL_FLOAT,           // type
                GL_FALSE,           // normalized?
                0,                  // stride
                (void*)0            // array buffer offset
        );

        // 4 attribute buffer : enemy_color_buffer
        glEnableVertexAttribArray(3);
        glBindBuffer(GL_ARRAY_BUFFER, enemy_color_buffer);
        glVertexAttribPointer(
                3,                  // attribute
                3,                  // size
                GL_FLOAT,           // type
                GL_FALSE,           // normalized?
                0,                  // stride
                (void*)0            // array buffer offset
        );

        glVertexAttribDivisor(0, 0);
        glVertexAttribDivisor(1, 1);
        glVertexAttribDivisor(2, 1);
        glVertexAttribDivisor(3, 0);

        glDrawArraysInstanced(GL_TRIANGLES, 0, 8*3, enemyContainer.size());

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
        glDisableVertexAttribArray(3);

        // Projectiles
        glUseProgram(programID2);
        glUniformMatrix4fv(MatrixID2, 1, GL_FALSE, &MVP[0][0]);

        // Bind our texture in Texture Unit 0
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, Texture);
        // Set our "myTextureSampler" sampler to use Texture Unit 0
        glUniform1i(TextureID, 0);

        // 1 attribute buffer : projectile_vertex_buffer
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, projectile_vertex_buffer);
        glVertexAttribPointer(
                0,                  // attribute
                3,                  // size
                GL_FLOAT,           // type
                GL_FALSE,           // normalized?
                0,                  // stride
                (void*)0            // array buffer offset
        );

        // 2 attribute buffer : projectile_position_buffer
        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, projectile_position_buffer);
        glVertexAttribPointer(
                1,                  // attribute
                3,                  // size
                GL_FLOAT,           // type
                GL_FALSE,           // normalized?
                0,                  // stride
                (void*)0            // array buffer offset
        );

        // 3 attribute buffer : projectile_uvbuffer
        glEnableVertexAttribArray(2);
        glBindBuffer(GL_ARRAY_BUFFER, projectile_uvbuffer);
        glVertexAttribPointer(
                2,                  // attribute
                2,                  // size
                GL_FLOAT,           // type
                GL_FALSE,           // normalized?
                0,                  // stride
                (void*)0            // array buffer offset
        );

        glVertexAttribDivisor(0, 0);
        glVertexAttribDivisor(1, 1);
        glVertexAttribDivisor(2, 0);

        glDrawArraysInstanced(GL_TRIANGLES, 0, vertices_proj.size(), projectileContainer.size());

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);

		// Swap buffers
		glfwSwapBuffers(window);
		glfwPollEvents();

        CheckCollision();
        DeleteDestroyedObject(enemyContainer);
        DeleteDestroyedObject(projectileContainer);
        DeleteFarProjectiles();


	} // Check if the ESC key was pressed or the window was closed
	while( glfwGetKey(window, GLFW_KEY_ESCAPE ) != GLFW_PRESS &&
		   glfwWindowShouldClose(window) == 0 );

	// Cleanup VBO and shader
	glDeleteBuffers(1, &enemy_vertex_buffer);
    glDeleteBuffers(1, &enemy_position_buffer);
    glDeleteBuffers(1, &enemy_rotation_axis_buffer);
    glDeleteBuffers(1, &enemy_color_buffer);

    glDeleteBuffers(1, &projectile_vertex_buffer);
    glDeleteBuffers(1, &projectile_position_buffer);
    glDeleteBuffers(1, &projectile_uvbuffer);

	glDeleteProgram(programID1);
	glDeleteProgram(programID2);
    glDeleteTextures(1, &Texture);
	glDeleteVertexArrays(1, &VertexArrayID);

	// Close OpenGL window and terminate GLFW
	glfwTerminate();

	return 0;
}

