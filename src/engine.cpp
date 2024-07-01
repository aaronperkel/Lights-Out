#include "engine.h"
#include <ctime>
#include <cstdlib>

enum state {start, play, over};
state screen;
int moves = 0;

Engine::Engine() : keys() {
    this->initWindow();
    this->initShaders();
    this->initShapes();
}

Engine::~Engine() {}

unsigned int Engine::initWindow(bool debug) {
    // glfw: initialize and configure
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE);
#endif
    glfwWindowHint(GLFW_RESIZABLE, false);

    window = glfwCreateWindow(width, height, "Lights Out!", nullptr, nullptr);
    glfwMakeContextCurrent(window);

    // glad: load all OpenGL function pointers
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        cout << "Failed to initialize GLAD" << endl;
        return -1;
    }

    // OpenGL configuration
    glViewport(0, 0, width, height);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glfwSwapInterval(1);

    return 0;
}

void Engine::initShaders() {
    // load shader manager
    shaderManager = make_unique<ShaderManager>();

    // Load shader into shader manager and retrieve it
    shapeShader = this->shaderManager->loadShader("../res/shaders/shape.vert", "../res/shaders/shape.frag",  nullptr, "shape");

    // Configure text shader and renderer
    textShader = shaderManager->loadShader("../res/shaders/text.vert", "../res/shaders/text.frag", nullptr, "text");
    fontRenderer = make_unique<FontRenderer>(shaderManager->getShader("text"), "../res/fonts/MxPlus_IBM_BIOS.ttf", 24);

    // Set uniforms
    textShader.setVector2f("vertex", vec4(100, 100, .5, .5));
    shapeShader.use();
    shapeShader.setMatrix4("projection", this->PROJECTION);
}

void Engine::initShapes() {
    // Light foreground
    const int SQUARE_SIDE = 100, GAP = 25, HOVER_BORDER_WIDTH = 10;
    int yPos = height - SQUARE_SIDE;
    for (int i = 0; i < 5; i++) {
        int xPos = SQUARE_SIDE;
        vector<unique_ptr<Shape>> row;
        for (int j = 0; j < 5; j++) {
            row.push_back(make_unique<Rect>(shapeShader, vec2{xPos, yPos}, vec2{SQUARE_SIDE, SQUARE_SIDE}, color{1, 1, 0, 1},1));
            xPos += SQUARE_SIDE + GAP;
        }
        yPos -= SQUARE_SIDE + GAP;
        // https://stackoverflow.com/questions/39724272/error-call-to-implicitly-deleted-copy-constructor-of-std-1unique-ptra-s
        lights.push_back(std::move(row));
    }

    // Hover
    for (int i = 0; i < 5; i++) {
        vector<unique_ptr<Shape>> row;
        for (int j = 0; j < 5; j++) {
            row.push_back(make_unique<Rect>(shapeShader, lights[i][j]->getPos(), vec2{SQUARE_SIDE + HOVER_BORDER_WIDTH, SQUARE_SIDE + HOVER_BORDER_WIDTH}, color (1, 0, 0, 0), 1));
        }
        // https://stackoverflow.com/questions/39724272/error-call-to-implicitly-deleted-copy-constructor-of-std-1unique-ptra-s
        lights_hover.push_back(std::move(row));
    }
    // https://www.geeksforgeeks.org/rand-and-srand-in-ccpp/#
    srand(time(0));
    int sum_cols;
    int sum_rows;

    // https://puzzling.stackexchange.com/questions/123075/how-do-i-determine-whether-a-5x5-lights-out-puzzle-is-solvable-without-trying-to
    do {
        sum_cols = 0;
        sum_rows = 0;
        for (int i = 0; i < 5; i++) {
            for (int j = 0; j < 5; j++) {
                // https://www.geeksforgeeks.org/rand-and-srand-in-ccpp/#
                if (rand() % 2 == 0) {
                    lights[i][j]->toggleStatus();
                }
            }
        }
        for (int i = 0; i < 5; i += 2) {
            sum_rows += lights[i][0]->getStatus() + lights[i][1]->getStatus() + lights[i][3]->getStatus() + lights[i][4]->getStatus();
        }

        for (int i = 0; i < 5; i += 2) {
            sum_cols += lights[0][i]->getStatus() + lights[1][i]->getStatus() + lights[3][i]->getStatus() + lights[4][i]->getStatus();
        }
    } while (sum_rows % 2 == 1 || sum_cols % 2 == 1);

}

void Engine::processInput() {
    glfwPollEvents();

    // Set keys to true if pressed, false if released
    for (int key = 0; key < 1024; ++key) {
        if (glfwGetKey(window, key) == GLFW_PRESS)
            keys[key] = true;
        else if (glfwGetKey(window, key) == GLFW_RELEASE)
            keys[key] = false;
    }

    // Close window if escape key is pressed
    if (keys[GLFW_KEY_ESCAPE])
        glfwSetWindowShouldClose(window, true);

    // Mouse position saved to check for collisions
    glfwGetCursorPos(window, &MouseX, &MouseY);

    // If we're in the start screen and the user presses s, change screen to play
    if (keys[GLFW_KEY_S] && screen == start) {
        screen = play;
        time_t curr_time;
        time(&curr_time);
        timer = (unsigned long)curr_time;
    }

    // Mouse position is inverted because the origin of the window is in the top left corner
    MouseY = height - MouseY; // Invert y-axis of mouse position
    bool mousePressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    if (screen == play) {
        // Checking for hover
        for (int i = 0; i < 5; i++) {
            for (int j = 0; j < 5; j++) {

                if (lights[i][j]->getStatus() == 1) {
                    lights[i][j]->setColor(color {1, 1, 0, 1});
                } else {
                    lights[i][j]->setColor(color {0.5, 0.5, 0.5, 1});
                }

                // if clicked
                if (lights[i][j]->isOverlapping(vec2{MouseX, MouseY})) {
                    lights_hover[i][j]->setOpacity(1);
                    if (mousePressedLastFrame && !mousePressed) {
                        moves++;
                        lights[i][j]->toggleStatus();
                        if (j + 1 < 5) {
                            lights[i][j+1]->toggleStatus();
                        }
                        if (j - 1 >= 0) {
                            lights[i][j-1]->toggleStatus();
                        }
                        if (i + 1 < 5) {
                            lights[i+1][j]->toggleStatus();
                        }
                        if (i - 1 >= 0) {
                            lights[i-1][j]->toggleStatus();
                        }
                    }
                } else {
                    lights_hover[i][j]->setOpacity(0);
                }
            }
        }
        bool done = true;
        for (int i = 0; i < 5; i++) {
            for (int j = 0; j < 5; j++) {
                if (lights[i][j]->getStatus() != 0) {
                    done = false;
                }
            }
        }
        if (done) {
            time_t end_time;
            time(&end_time);
            timer = (unsigned long)end_time - timer;
            screen = over;
        }
    }

    // Save mousePressed for next frame
    mousePressedLastFrame = mousePressed;

}

void Engine::update() {
    // Calculate delta time
    float currentFrame = glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;
}

void Engine::render() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Set background color
    glClear(GL_COLOR_BUFFER_BIT);

    // Set shader to use for all shapes
    shapeShader.use();

    switch (screen) {
        case (start): {
            string message = "Press s to start",
                    game_desc1 = "Click all of the yellow lights until they",
                    game_desc2 = "all turn gray! Be careful, the surrounding",
                    game_desc3 = "lights turn on or off depending on ",
                    game_desc4 = "their state when you click!";

            // (12 * message.length()) is the offset to center text.
            // 12 pixels is the width of each character scaled by 1.
            this->fontRenderer->renderText(message, width / 2 - (12 * message.length()), height / 2, 1, vec3{0, .9, 0});
            this->fontRenderer->renderText(game_desc1, width / 2 - (12 * message.length()), height / 2 - 30, .5, vec3{1, 1, 1});
            this->fontRenderer->renderText(game_desc2, width / 2 - (12 * message.length()), height / 2 - 30 - 20, .5, vec3{1, 1, 1});
            this->fontRenderer->renderText(game_desc3, width / 2 - (12 * message.length()), height / 2 - 30 - 40, .5, vec3{1, 1, 1});
            this->fontRenderer->renderText(game_desc4, width / 2 - (12 * message.length()), height / 2 - 30 - 60, .5, vec3{1, 1, 1});

            break;
        }
        case (play): {
            string message = "Moves: " + std::to_string(moves);
            // draw red outline
            for (vector<unique_ptr<Shape>> &row: lights_hover) {
                for (unique_ptr<Shape> &square: row) {
                    square->setUniforms();
                    square->draw();
                }
            }
            // draw yellow square
            for (vector<unique_ptr<Shape>> &row: lights) {
                for (unique_ptr<Shape> &square: row) {
                    square->setUniforms();
                    square->draw();
                }
            }
            this->fontRenderer->renderText(message, 25, 15, 0.6, vec3 {1, 1, 1});
            break;
        }
        case (over): {
            string message = "You win!";
            string final_time = "You finished in " + std::to_string(timer) + " seconds";
            string final_clicks = "with " + std::to_string(moves) + " clicks!";
            fontRenderer->renderText(message, width / 2 - (12 * message.length()), height / 2, 1, vec3 {1, 1, 1});
            fontRenderer->renderText(final_time, width / 2 - (12 * message.length()), height / 2 - 30, .6, vec3 {1, 1, 1});
            fontRenderer->renderText(final_clicks, width / 2 - (12 * message.length()), height / 2 - 60, .6, vec3 {1, 1, 1});

            break;
        }
    }

    glfwSwapBuffers(window);
}

bool Engine::shouldClose() {
    return glfwWindowShouldClose(window);
}

GLenum Engine::glCheckError_(const char *file, int line) {
    GLenum errorCode;
    while ((errorCode = glGetError()) != GL_NO_ERROR) {
        string error;
        switch (errorCode) {
            case GL_INVALID_ENUM:                  error = "INVALID_ENUM"; break;
            case GL_INVALID_VALUE:                 error = "INVALID_VALUE"; break;
            case GL_INVALID_OPERATION:             error = "INVALID_OPERATION"; break;
            case GL_STACK_OVERFLOW:                error = "STACK_OVERFLOW"; break;
            case GL_STACK_UNDERFLOW:               error = "STACK_UNDERFLOW"; break;
            case GL_OUT_OF_MEMORY:                 error = "OUT_OF_MEMORY"; break;
            case GL_INVALID_FRAMEBUFFER_OPERATION: error = "INVALID_FRAMEBUFFER_OPERATION"; break;
        }
        cout << error << " | " << file << " (" << line << ")" << endl;
    }
    return errorCode;
}