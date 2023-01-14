
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

struct Fox {
    Vector3 position;
    float rotation;

    Vector3 velocity;
    float   rotationSpeed;

    BoundingBox bounds;

    Texture2D texture;
};

struct FoxAnimationState {
    float time;
    bool isJumping;

    float waitTime;
    int jumps;
};

// ================
// Config 
// ================
const int screenWidth = 1600;
const int screenHeight = 900;

const int fontSize = 70;

const float minHitSpeed = 40;

// Hand stuff
const Vector3 handDefaultPosition = {5, 1, 0};
const Vector3 handDefaultRotation = {0, PI / 2, PI / 2};
const float handScale = 0.6f;
const float handRotationFactor = 0.05f;
const float handDampFactor = 20;

// Fox
const float foxScale = 3;
const Vector3 foxStartPosition = {0, foxScale / 2, 0};

// Fox jump animation
const int jumpsMin = 1;
const int jumpsMax = 3;
const float jumpTimeMin = 1.0f;
const float jumpTimeMax = 2.0f;
const float foxJumpHeight = 0.3f;
const float foxJumpTime = 0.38f;

const float gravity = -9.81f;

// Screams 
const float attenuationFactor = 24;
const float pitchVariation = 0.08f;

// ================
// Menu
// ================
const Vector2 menuRectOffset = { 0, 220 };
const Vector2 menuRectSize = { 810, 330 };
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

Thank you Tenma for amazing screams!
)###";

// ================
// Assets
// ================ 
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

// ================
// Game State
// ================
Fox fox;
FoxAnimationState foxAnimationState;

bool handGrabbed;
bool foxHit;

Vector3 pointerPosition;
Vector3 previousPointerPos;

Vector3 handPosition;
float handSpeed;
Vector3 targetHandRotation;
Vector3 currentHandRotation = handDefaultRotation;

char resultText[256];
int resultTextWidth;

bool isInMenu = true;
bool isPatting;

Camera camera;

// ================
// Operators
// ================
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

// ================
// Helper Functions 
// ================

// small hack, since it will work only for types that
// can be compared to 0, typically numerical, but oh well
// that's what I need
template <typename T> 
int sign(T val) {
    return (0 < val) - (val < 0);
}

float RandomRange(float a, float b) {
    float p = (float) rand() / (float) RAND_MAX;
    return Lerp(a, b, p);
}

int RandomRange(int a, int b) {
    float p = (float) rand() / (float) RAND_MAX;
    return (int) (Lerp((float)a, (float)b, p) + 0.5f);
}

////
void UpdateDrawFrame();

void UpdateMenu();
void UpdateGame();

void DrawMenu();
void DrawGame();

void FoxAnimationRoutine(FoxAnimationState*);


int main()
{
    InitWindow(screenWidth, screenHeight, "Spank The Fox");
    InitAudioDevice();

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
    screamSoundThresholds[3] = 350;

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
    camera.position = Vector3{ 2.0f, 1.0f, 6.0f };
    camera.target = camera.position + Vector3{0, 0, -1};
    camera.up = Vector3{ 0.0f, 1.0f, 0.0f };

    camera.fovy = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // Prepare game state
    fox.position = foxStartPosition;
    handPosition = handDefaultPosition;

    Vector3 min = Vector3{1, 1, 1} * -foxScale / 2 + foxStartPosition;
    Vector3 max = Vector3{1, 1, 1} *  foxScale / 2 + foxStartPosition;
    fox.bounds  = { min, max };

    PlayMusicStream(music);

    /// 
    // Main Loop
    ///

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

    FoxAnimationRoutine(&foxAnimationState);

    // Rendering
    BeginDrawing();
    ClearBackground({219, 216, 225, 0});
    // ClearBackground({242, 159, 203, 0});
    // ClearBackground(LIGHTGRAY);

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
    // Create ray from cursor point, using current camera
    Ray ray = GetMouseRay(GetMousePosition(), camera);

    // Find distance on the ray, on which, the Z coordinate is 0.
    // In the other words, cast ray on the 2D plane XY
    float dist = -ray.position.z / ray.direction.z;
    pointerPosition = ray.position + ray.direction * dist;

    if(handGrabbed == false) {
        handPosition = Vector3Lerp(handPosition, handDefaultPosition, GetFrameTime() * 4);
        targetHandRotation = handDefaultRotation;

        if(IsMouseButtonPressed(0)) {
            handGrabbed = true;

            HideCursor();
        }
    }
    else if(foxHit == false) {
        // Calculate speed of the pointer in the World coordinates
        Vector3 delta = pointerPosition - previousPointerPos;
        Vector3 velocity = delta / GetFrameTime();
        handSpeed = Vector3Length(delta) / GetFrameTime() * -sign(delta.x);

        handPosition = pointerPosition;

        // Calculate target hand rotation using direction from the camera, 
        // it's used so the hand is rotated away from our view
        // should look better
        float rayAngle = atan2f(ray.direction.z, ray.direction.x) + PI / 2.0f;
        targetHandRotation.y = velocity.x * handRotationFactor - rayAngle;

        // Actual logic that handles hitting
        if(isPatting == false && handPosition.x < fox.position.x && handSpeed > minHitSpeed) {
            snprintf(resultText, sizeof(resultText), "YOU SPANKED THE FOX AT\n%d KILOMETERS PER HOUR", (int) handSpeed);
            resultTextWidth = MeasureText(resultText, fontSize);

            foxHit = true;

            currentHitSoundIndex = rand() % HitSoundsCount;
            PlaySound(hitSounds[currentHitSoundIndex]);

            currentScreamIndex = ScreamSoundsCount - 1;
            for(int i = 0; i < ScreamSoundsCount - 1; i++) {
                if(handSpeed >= screamSoundThresholds[i] && handSpeed < screamSoundThresholds[i + 1]) {
                    currentScreamIndex = i;
                    break;
                }
            }

            // small pitch variation to hide a little repetiveness
            // of the screams
            float r = RandomRange(-pitchVariation, pitchVariation);
            float pitch  = 1 + r;

            SetSoundPitch(screamSounds[currentScreamIndex], pitch);
            PlaySound(screamSounds[currentScreamIndex]);

            StopMusicStream(music);

            fox.velocity = velocity;
        }


        bool isInBounds = handPosition.x > fox.bounds.min.x && handPosition.x < fox.bounds.max.x &&
                          handPosition.y > fox.bounds.min.y && handPosition.y < fox.bounds.max.y; 

        if(handPosition.x < fox.bounds.max.x && handSpeed < minHitSpeed) {
            if(isInBounds)
            {
                isPatting = true;

                targetHandRotation.x = 30 * DEG2RAD;
                targetHandRotation.z = 180 * DEG2RAD;
            }
        }
        
        if(isPatting && isInBounds == false) {
            isPatting = false;

            // targetHandRotation.x = 0;
            // targetHandRotation.z = 90 * DEG2RAD;
            targetHandRotation = handDefaultRotation;
        }
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
        // now it's used for sound attenuation
        fox.velocity = fox.velocity + Vector3{0, 0.5f * gravity * GetFrameTime(), 0};
        fox.position = fox.position + fox.velocity * GetFrameTime();

        // Sound attenuation mentioned earlier. It uses simple 1 / x function
        // multiplied by found factor to "feel" better
        float distFromCenter = Vector3Length(fox.position);
        float volume = attenuationFactor / distFromCenter;
        // Clamp sound volume so it never exceeds 1.
        volume = volume > 1 ? 1 : volume;
        SetSoundVolume(screamSounds[currentScreamIndex], volume);

        // Last minute change, when after hit, the hand also move
        // since it's last minute, I don't store actual hand velocity
        // and just usuning fox velocity
        // Since it's on the screen for fraction of a second it's 
        // "good enough"
        handPosition = handPosition + fox.velocity * GetFrameTime();
    }

    // Reset Game State
    if(foxHit && IsMouseButtonPressed(0)) {
        foxHit = false;
        handSpeed = 0;
        resultText[0] = '\0';

        fox.position = foxStartPosition;
        fox.velocity = {0, 0, 0};

        StopSound(screamSounds[currentScreamIndex]);
        StopSound(hitSounds[currentHitSoundIndex]);
        PlayMusicStream(music);
    }

    if(IsMouseButtonReleased(0)) {
        handGrabbed = false;
        isPatting = false;

        targetHandRotation = handDefaultRotation;

        ShowCursor();
    }

    // calculate hand roation. Here I use very simple damping methos using Lerp function to give it
    // less jerky movement. Wraning! This method is not framerate-independent, even if you multiply
    // the factor by frame time. It's also unstable in certain situations
    // check: https://theorangeduck.com/page/spring-roll-call
    currentHandRotation = Vector3Lerp(currentHandRotation, targetHandRotation, GetFrameTime() * handDampFactor);

    // Clamp Y rotation to 90 degrees
    currentHandRotation.y = fminf(currentHandRotation.y, PI / 2);

    previousPointerPos = pointerPosition;
}

void DrawMenu() {
    DrawGame();

    // Pretty much eyeballing the values here so most of them aren't in the config
    Vector2 screenCenter = Vector2{screenWidth, screenHeight} / 2;

    int w = MeasureText(titleText, titleSize);
    DrawText(titleText, (int) screenCenter.x - w / 2, 20, titleSize, BLACK);

    w = MeasureText(warningText, warningSize);
    DrawText(warningText, (int) screenCenter.x - w / 2, 190, warningSize, DARKGRAY);

    Vector2 rectPos = screenCenter - menuRectSize / 2 + menuRectOffset;
    Vector2 rectCenter = rectPos + menuRectSize / 2;
    DrawRectangleV(rectPos, menuRectSize, {0, 0, 0, 127});

    w = MeasureText(creditsLabelText, creditsLabelSize);
    DrawText(creditsLabelText, (int) rectCenter.x - w / 2, (int) rectPos.y + 10, creditsLabelSize, LIGHTGRAY);

    DrawText(creditsText, (int) rectPos.x + creditsMargin, (int) rectPos.y + 90, creditsSize, LIGHTGRAY);
}

void DrawGame() {
    BeginMode3D(camera);

    // Disable writing to depth buffer, so grid and fox quad 
    // won't render above the hand
    rlDisableDepthMask();

    DrawGrid(15, 15);
    DrawBillboard(camera, fox.texture, fox.position, foxScale, WHITE);

    // DrawBoundingBox(fox.bounds, RED);

    // DrawGrid and DrawBillboard is batched, so we have to flush it before
    // rendering hand model to get proper draw order
    rlDrawRenderBatchActive();

    // Enable depth buffer again so the hand renders correctly
    rlEnableDepthMask();

    Matrix handTransform = MatrixScale(handScale, handScale, handScale);
    
    handTransform = handTransform * 
                    MatrixTranslate(0, 0, -1.2f) * // Move pivot to the beginning of the hand
                    MatrixRotateXYZ(currentHandRotation);

    handTransform = handTransform * MatrixTranslate(handPosition.x, handPosition.y, handPosition.z);

    DrawMesh(handMesh, handMaterial, handTransform);

    EndMode3D();

    DrawText(resultText, (screenWidth - resultTextWidth) / 2, screenHeight / 2 - 70, fontSize, BLACK);
}

// Coroutine style animation routine
void FoxAnimationRoutine(FoxAnimationState* data) {
    if(data->isJumping == false) {
        if(isPatting) {
            return;
        }

        data->time += GetFrameTime();
        if(data->time >= data->waitTime) {
            data->isJumping = true;
            data->time = 0;

            data->jumps = RandomRange(jumpsMin, jumpsMax);
        }
    }
    else {
        data->time += GetFrameTime();

        fox.position.y = foxStartPosition.y + foxJumpHeight * fabsf(sinf(data->time * PI / foxJumpTime));

        if(data->time >= data->jumps * foxJumpTime) {
            data->isJumping = false;
            data->time = 0;

            data->waitTime = RandomRange(jumpTimeMin, jumpTimeMax);
        }
    }
}