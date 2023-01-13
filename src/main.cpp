
#if WEB_BUILD
#include <emscripten/emscripten.h>
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

#include "include/raylib.h"
#include "include/rlgl.h"

#define RAYMATH_IMPLEMENTATION
#include "include/raymath.h"

// TODO:
// more randomized hit sounds
// Fox screams
// Volume controls (can I do it from html?)
// sound attenuation
// credits
// comment code 
// repo and deploy

struct Fox {
    Vector3 position;
    float rotation;

    Vector3 velocity;
    float   rotationSpeed;

    Texture2D texture;
};

// Config 
const int screenWidth = 1600;
const int screenHeight = 900;

const int fontSize = 70;

const float minHitSpeed = 20;

const Vector3 handDefaultPosition = {5, 1, 0};
const float handDefaultRotation = PI / 2;
const float handScale = 0.6f;
const float handRotationFactor = 0.05f;
const float handDampFactor = 20;

const Vector3 foxStartPosition = {0, 1.5f, 0};
const float foxScale = 3;

const float foxJumpHeight = 0.25f;
const float foxJumpSpeed = 8.0f;

const float gravity = -9.81f;

const float attenuationFactor = 24;

// Menu stuff
const Vector2 menuRectOffset = { 0, 220 };
const Vector2 menuRectSize = { 700, 300 };
const int titleSize = 110;
char titleText[] = "Spank the fox";

const int warningSize = 40;
char warningText[] = "You might want to lower your volume...";

const int creditsLabelSize = 50;
char creditsLabelText[] = "Credits:";

const int creditsMargin = 4;
const int creditsSize = 40;
char creditsText[] = R"###(Programing and "music": Coedo
Fox art: Sick2day
Screams: Tenma Maemi <3
)###";

// Assets 
Model hand;
Mesh handMesh;
Material handMaterial;

Texture2D handTexture;

// Sounds
const int HitSoundsCount = 3;
Sound hitSounds[HitSoundsCount];
int currentHitSoundIndex;

const int ScreamSoundsCount = 4;
Sound screamSounds[ScreamSoundsCount];
int   screamSoundThresholds[ScreamSoundsCount];
int   currentScreamIndex;

Music music;

// Game State
Fox fox;

bool handGrabbed;
bool foxHit;

Vector3 handPosition;
Vector3 handPrevPosition;
float handSpeed;
float targetHandRotation;
float currentHandRotation;

char resultText[256];
int resultTextWidth;

bool isInMenu = true;

Camera camera;

////
void UpdateDrawFrame();

void UpdateMenu();
void UpdateGame();

void DrawMenu();
void DrawGame();

// Operators
Vector3 operator+(Vector3 a, Vector3 b) {
    return Vector3Add(a, b);
}

Vector3 operator-(Vector3 a, Vector3 b) {
    return Vector3Subtract(a, b);
}

Vector3 operator*(Vector3 v, float s) {
    return {v.x * s, v.y * s, v.z * s};
}

Vector3 operator/(Vector3 v, float s) {
    return {v.x / s, v.y / s, v.z / s};
}

Vector2 operator+(Vector2 a, Vector2 b) {
    return Vector2Add(a, b);
}

Vector2 operator-(Vector2 a, Vector2 b) {
    return Vector2Subtract(a, b);
}

Vector2 operator*(Vector2 v, float s) {
    return {v.x * s, v.y * s};
}

Vector2 operator/(Vector2 v, float s) {
    return {v.x / s, v.y / s};
}

Matrix operator*(Matrix a, Matrix b) {
    return MatrixMultiply(a, b);
}

// helpers

// small hack, since it will work only for types that
// can be compared to 0, typically numerical, but oh well
// that's what I need
template <typename T> 
int sign(T val) {
    return (0 < val) - (val < 0);
}

int main()
{
    // Init Raylib stuff
    InitWindow(screenWidth, screenHeight, "Spank The Fox");
    InitAudioDevice();

    // Load Assets
    handTexture = LoadTexture("assets/hand.png");
    fox.texture = LoadTexture("assets/fox.png");

    srand((unsigned int) time(NULL));

    char temp[128];
    for(int i = 0; i < 3; i++) {
        snprintf(temp, sizeof(temp), "assets/hit%d.mp3", i);
        hitSounds[i] = LoadSound(temp);
    }

    for(int i = 0; i < ScreamSoundsCount; i++) {
        snprintf(temp, sizeof(temp), "assets/scream%d.wav", i);
        screamSounds[i] = LoadSound(temp);
    }

    screamSoundThresholds[0] = 0;
    screamSoundThresholds[1] = 100;
    screamSoundThresholds[2] = 200;
    screamSoundThresholds[3] = 300;

    music = LoadMusicStream("assets/music.mp3");

    // Load and ensure that hand model was loaded succesfully,
    // otherwise crash program. If we wouldn't check, next lines
    // would buffer overflow what could cause unspecified behaviour
    hand = LoadModel("assets/hand.obj");
    assert(hand.meshCount == 1 && hand.materialCount == 1);

    handMesh = hand.meshes[0];
    handMaterial = hand.materials[0];
    // There are some problems with loading .obj material file
    // so we load and set texture manually 
    handMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = handTexture;

    // Setup camera
    camera.position = Vector3{ 2.0f, 1.0f, 7.0f };
    camera.target = camera.position + Vector3{0, 0, -1};
    camera.up = Vector3{ 0.0f, 1.0f, 0.0f };

    camera.fovy = 40.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // Prepare game state
    fox.position = foxStartPosition;
    handPosition = handDefaultPosition;

    PlayMusicStream(music);

    // Main game loop
#if WEB_BUILD
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
    SetTargetFPS(60);
    while(WindowShouldClose() == false) {
        UpdateDrawFrame();
    }
#endif

    CloseWindow();
    return 0;
}

void UpdateDrawFrame()
{
    // Update
    UpdateMusicStream(music);

    if(isInMenu) {
        UpdateMenu();
    }
    else {
        UpdateGame();
    }

    if(foxHit == false) {
        fox.position.y = foxStartPosition.y + foxJumpHeight * fabsf(sinf((float) GetTime() * foxJumpSpeed));
    }

    // Rendering
    BeginDrawing();
    ClearBackground(LIGHTGRAY);

    if(isInMenu) {
        DrawMenu();
    }
    else {
        DrawGame();
    }

    EndDrawing();
}

void UpdateMenu() {
    if(IsMouseButtonPressed(0)) {
        isInMenu = false;
    }
}

void UpdateGame() {
    if(handGrabbed == false) {
        if(IsMouseButtonPressed(0)) {
            handGrabbed = true;
            HideCursor();
        }
    }

    if(handGrabbed) {
        // Create ray from cursor point, using current camera
        Ray ray = GetMouseRay(GetMousePosition(), camera);

        // fint distance on the ray, on which, the Z coordinate is 0.
        // In other words, cast ray on the 2D plane XY
        float dist = -ray.position.z / ray.direction.z;
        // Then calculate actual point 
        handPosition = ray.position + ray.direction * dist;

        // Calculate speed of the pointer in Wold coordinates
        Vector3 delta = handPosition - handPrevPosition;
        handSpeed = Vector3Length(delta) / GetFrameTime() * -sign(delta.x);

        // Calculate target hand rotation using direction from the camera, 
        // it's used so the hand is rotated away from our view
        // should look better
        float rayAngle = atan2f(ray.direction.z, ray.direction.x) + PI / 2.0f;
        targetHandRotation = -handSpeed * handRotationFactor - rayAngle;

        // Actual logic that handles hitting
        if(foxHit == false && handPosition.x < 0 && handSpeed > minHitSpeed) {
            snprintf(resultText, sizeof(resultText), "YOU SPANKED THE FOX AT\n%d KILOMETERS PER HOUR", (int) handSpeed);
            resultTextWidth = MeasureText(resultText, fontSize);

            foxHit = true;

            currentHitSoundIndex = rand() % HitSoundsCount;
            PlaySound(hitSounds[currentHitSoundIndex]);

            currentScreamIndex = ScreamSoundsCount - 1;
            for(int i = 0; i < ScreamSoundsCount - 1; i++) {
                if(handSpeed >= screamSoundThresholds[0] && handSpeed < screamSoundThresholds[i + 1]) {
                    currentScreamIndex = i;
                    break;
                }
            }

            PlaySound(screamSounds[currentScreamIndex]);

            StopMusicStream(music);

            fox.velocity = delta / GetFrameTime();
        }

        if(IsMouseButtonReleased(0)) {
            handGrabbed = false;
            ShowCursor();
        }

        handPrevPosition = handPosition;
    }

    // I know I could just use 'else' but for the 
    // sake of readibility I will just separate it
    if(handGrabbed == false) {
        handPosition = Vector3Lerp(handPosition, handDefaultPosition, GetFrameTime() * 3);
        targetHandRotation = handDefaultRotation;
    }

    if(foxHit) {
        // simple kinematic equations calculating fox velocity and positions.
        // Those are:
        // V = V0 + 1/2 * a * t
        // S = S0 + V * t
        // where:
        // V = velocity
        // a = acceleration (in this case gravity)
        // t = time interval
        // S = position
        // They are here because at some point I wanted for camera to track the Fox
        // but it was cut out to save "development" time
        // 
        // now it's used for "realistic" sound attenuation
        fox.velocity = fox.velocity + Vector3{0, 0.5f * gravity * GetFrameTime(), 0};
        fox.position = fox.position + fox.velocity * GetFrameTime();

        // Sound attenuation mentioned earlier. It uses simple 1 / x function
        // multiplied by found factor to "feel" better
        float distFromCenter = Vector3Length(fox.position);
        float volume = attenuationFactor / distFromCenter;
        // Clamp sound volume so it never exceeds 1.
        volume = volume > 1 ? 1 : volume;
        SetSoundVolume(screamSounds[currentScreamIndex], volume);

    }

    // Reset Game State
    if(foxHit && IsMouseButtonPressed(0)) {
        foxHit = false;
        handSpeed = 0;
        resultText[0] = '\0';

        fox.position = foxStartPosition;
        fox.velocity = {0, 0, 0};

        camera.target = camera.position + Vector3{0, 0, -1};

        StopSound(screamSounds[currentScreamIndex]);
        StopSound(hitSounds[currentHitSoundIndex]);
        PlayMusicStream(music);
    }

    // calculate hand roation. Here I use very simple damping methos using Lerp function to give it
    // less jerky movement. Wraning! This method is not framerate-independent, even if you multiply
    // the factor by frame time. It's also unstable in certain situations
    // check: https://theorangeduck.com/page/spring-roll-call
    currentHandRotation = Lerp(currentHandRotation, targetHandRotation, GetFrameTime() * handDampFactor);
    // Clamp rotation to 90 degrees
    currentHandRotation = fminf(currentHandRotation, PI / 2);
}

void DrawMenu() {
    DrawGame();

    Vector2 screenCenter = Vector2{screenWidth, screenHeight} / 2;

    int w = MeasureText(titleText, titleSize);
    DrawText(titleText, (int) screenCenter.x - w / 2, 20, titleSize, BLACK);

    w = MeasureText(warningText, warningSize);
    DrawText(warningText, (int) screenCenter.x - w / 2, 190, warningSize, DARKGRAY);

    Vector2 rectCenter = screenCenter + menuRectOffset;
    Vector2 rectPos = screenCenter - menuRectSize / 2 + menuRectOffset;
    DrawRectangleV(rectPos, menuRectSize, {0, 0, 0, 127});

    w = MeasureText(creditsLabelText, creditsLabelSize);
    DrawText(creditsLabelText, (int) rectCenter.x - w / 2, (int) rectPos.y + 10, creditsLabelSize, LIGHTGRAY);

    DrawText(creditsText, (int) rectPos.x + creditsMargin, (int) rectPos.y + 90, creditsSize, LIGHTGRAY);
}

void DrawGame() {
    BeginMode3D(camera);
    // I'm doing manual sorting without depth buffer
    rlDisableDepthTest();
    rlDisableDepthMask();

    DrawGrid(10, 10.0f);
    DrawBillboard(camera, fox.texture, fox.position, foxScale, WHITE);

    // DrawGrid and DrawBillboard is batched, so we have to flush it before
    // rendering hand model to get proper draw order
    rlDrawRenderBatchActive();

    Matrix handTransform = MatrixScale(handScale, handScale, handScale);
    // Move pivot to the beggining of the hand
    handTransform = handTransform * MatrixTranslate(0, 0, -1.2f);
    handTransform = handTransform * 
                    MatrixRotate({0, 0, 1}, 90 * DEG2RAD) * // rotate do the hand stays vertical
                    MatrixRotate({0, 1, 0}, currentHandRotation); // rotation caused by movement
    handTransform = handTransform * MatrixTranslate(handPosition.x, handPosition.y, handPosition.z);

    DrawMesh(handMesh, handMaterial, handTransform);

    EndMode3D();

    DrawText(resultText, (screenWidth - resultTextWidth) / 2, screenHeight / 2 - 70, fontSize, BLACK);
}
