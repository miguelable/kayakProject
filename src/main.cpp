#include <Arduino.h>
#include <NeoPixelAnimator.h>
#include <NeoPixelBus.h>

#define BUTTON_PIN GPIO_NUM_13
uint8_t program = 0;

const uint16_t PixelCount        = 300;                    // make sure to set this to the number of pixels in your strip
const uint8_t  PixelPin          = 4;                      // make sure to set this to the correct pin, ignored for Esp8266
const uint8_t  AnimationChannels = 1;                      // we only need one as all the pixels are animated at once
const uint16_t AnimCount         = PixelCount / 5 * 2 + 1; // we only need enough animations for the tail and one extra
const uint16_t PixelFadeDuration = 300;                    // third of a second
const uint16_t TailLength        = 20;                     // length of the tail, must be shorter than PixelCount
const float    MaxLightness      = 0.4f;                   // max lightness at the head of the tail (0.5f is full bright)

// one second divide by the number of pixels = loop once a second
const uint16_t NextPixelMoveDuration = 2000 / PixelCount; // how fast we move through the pixels

NeoGamma<NeoGammaTableMethod>                colorGamma;
NeoPixelBus<NeoGrbFeature, NeoWs2812xMethod> strip(PixelCount, PixelPin);
NeoPixelAnimator                             animations(PixelCount);
RgbColor                                     CylonEyeColor(HtmlColor(0x7f0000));

// NeoPixelAnimator animations(2); // only ever need 2 animations
uint16_t         lastPixel = 0; // track the eye position
int8_t           moveDir   = 1; // track the direction of movement
AnimEaseFunction moveEase  = NeoEase::SinusoidalInOut;

boolean fadeToColor = true; // general purpose variable used to store effect state
struct MyAnimationState
{
  RgbColor StartingColor;
  RgbColor EndingColor;
  uint16_t IndexPixel;
};
MyAnimationState animationState[AnimCount];
MyAnimationState animationStateTwo[PixelCount];
uint16_t         frontPixel = 0; // the front of the loop
RgbColor         frontColor;     // the color at the front of the loop

// Led Task Handle
TaskHandle_t ledTaskHandle = NULL;

void FadeAll(uint8_t darkenBy)
{
  RgbColor color;
  for (uint16_t indexPixel = 0; indexPixel < strip.PixelCount(); indexPixel++) {
    color = strip.GetPixelColor<RgbColor>(indexPixel);
    color.Darken(darkenBy);
    strip.SetPixelColor(indexPixel, color);
  }
}

void FadeAnimUpdate(const AnimationParam& param)
{
  if (param.state == AnimationState_Completed) {
    FadeAll(10);
    if (program == 1)
      animations.RestartAnimation(param.index);
    else
      animations.StopAnimation(param.index);
  }
}

void MoveAnimUpdate(const AnimationParam& param)
{
  // apply the movement animation curve
  float progress = moveEase(param.progress);

  // use the curved progress to calculate the pixel to effect
  uint16_t nextPixel;
  if (moveDir > 0)
    nextPixel = progress * PixelCount;
  else
    nextPixel = (1.0f - progress) * PixelCount;

  if (lastPixel != nextPixel)
    for (uint16_t i = lastPixel + moveDir; i != nextPixel; i += moveDir)
      strip.SetPixelColor(i, CylonEyeColor);
  strip.SetPixelColor(nextPixel, CylonEyeColor);

  lastPixel = nextPixel;

  if (param.state == AnimationState_Completed) {
    // reverse direction of movement
    moveDir *= -1;

    // Change the color for the next movement randomly
    CylonEyeColor = RgbColor(random(255), random(255), random(255));

    if (program == 1)
      animations.RestartAnimation(param.index);
    else
      animations.StopAnimation(param.index);
  }
}

void BlendAnimUpdate(const AnimationParam& param)
{
  RgbColor updatedColor = RgbColor::LinearBlend(animationState[param.index].StartingColor, animationState[param.index].EndingColor, param.progress);

  // apply the color to the strip
  for (uint16_t pixel = 0; pixel < PixelCount; pixel++) {
    strip.SetPixelColor(pixel, updatedColor);
  }
}

void BlendAnimUpdateTwo(const AnimationParam& param)
{
  RgbColor updatedColor =
    RgbColor::LinearBlend(animationStateTwo[param.index].StartingColor, animationStateTwo[param.index].EndingColor, param.progress);
  // apply the color to the strip
  strip.SetPixelColor(param.index, updatedColor);
}

void PickRandom(float luminance)
{
  // pick random count of pixels to animate
  uint16_t count = random(PixelCount);
  while (count > 0) {
    // pick a random pixel
    uint16_t pixel = random(PixelCount);

    // pick random time and random color
    // we use HslColor object as it allows us to easily pick a color
    // with the same saturation and luminance
    uint16_t time                          = random(100, 400);
    animationStateTwo[pixel].StartingColor = strip.GetPixelColor<RgbColor>(pixel);
    animationStateTwo[pixel].EndingColor   = HslColor(random(360) / 360.0f, 1.0f, luminance);

    animations.StartAnimation(pixel, time, BlendAnimUpdateTwo);

    count--;
  }
}

void FadeInFadeOutRinseRepeat(float luminance)
{
  if (fadeToColor) {
    RgbColor target                 = HslColor(random(360) / 360.0f, 1.0f, luminance);
    uint16_t time                   = random(800, 2000);
    animationState[0].StartingColor = strip.GetPixelColor<RgbColor>(0);
    animationState[0].EndingColor   = target;
    animations.StartAnimation(0, time, BlendAnimUpdate);
  }
  else {
    uint16_t time                   = random(600, 700);
    animationState[0].StartingColor = strip.GetPixelColor<RgbColor>(0);
    animationState[0].EndingColor   = RgbColor(0);
    animations.StartAnimation(0, time, BlendAnimUpdate);
  }
  fadeToColor = !fadeToColor;
}

void FadeOutAnimUpdate(const AnimationParam& param)
{
  RgbColor updatedColor = RgbColor::LinearBlend(animationState[param.index].StartingColor, animationState[param.index].EndingColor, param.progress);
  // apply the color to the strip
  strip.SetPixelColor(animationState[param.index].IndexPixel, colorGamma.Correct(updatedColor));
}

void LoopAnimUpdate(const AnimationParam& param)
{
  if (param.state == AnimationState_Completed) {
    // done, time to restart this position tracking animation/timer
    animations.RestartAnimation(param.index);

    // pick the next pixel inline to start animating
    //
    frontPixel = (frontPixel + 1) % PixelCount; // increment and wrap
    if (frontPixel == 0) {
      // we looped, lets pick a new front color
      frontColor = HslColor(random(360) / 360.0f, 1.0f, 0.25f);
      // check if program has changed
      if (program != 3) {
        animations.StopAnimation(param.index);
        return;
      }
    }

    uint16_t indexAnim;
    // do we have an animation available to use to animate the next front pixel?
    // if you see skipping, then either you are going to fast or need to increase
    // the number of animation channels
    if (animations.NextAvailableAnimation(&indexAnim, 1)) {
      animationState[indexAnim].StartingColor = frontColor;
      animationState[indexAnim].EndingColor   = RgbColor(0, 0, 0);
      animationState[indexAnim].IndexPixel    = frontPixel;

      animations.StartAnimation(indexAnim, PixelFadeDuration, FadeOutAnimUpdate);
    }
  }
}

void SetRandomSeed()
{
  uint32_t seed;
  seed = analogRead(0);
  delay(1);
  for (int shifts = 3; shifts < 31; shifts += 3) {
    seed ^= analogRead(0) << shifts;
    delay(1);
  }
  randomSeed(seed);
}

void SetupAnimationSet()
{
  // setup some animations
  for (uint16_t pixel = 0; pixel < PixelCount; pixel++) {
    const uint8_t    peak          = 128;
    uint16_t         time          = random(500, 800);
    RgbColor         originalColor = strip.GetPixelColor<RgbColor>(pixel);
    RgbColor         targetColor   = RgbColor(random(peak), random(peak), random(peak));
    AnimEaseFunction easing;
    switch (random(3)) {
      case 0:
        easing = NeoEase::CubicIn;
        break;
      case 1:
        easing = NeoEase::CubicOut;
        break;
      case 2:
        easing = NeoEase::QuadraticInOut;
        break;
    }
    AnimUpdateCallback animUpdate = [=](const AnimationParam& param) {
      float    progress     = easing(param.progress);
      RgbColor updatedColor = RgbColor::LinearBlend(originalColor, targetColor, progress);
      strip.SetPixelColor(pixel, updatedColor);
    };
    animations.StartAnimation(pixel, time, animUpdate);
  }
}

void SetupAnimations()
{
  // fade all pixels providing a tail that is longer the faster
  // the pixel moves.
  animations.StartAnimation(0, 20, FadeAnimUpdate);

  // take several seconds to move eye fron one side to the other
  animations.StartAnimation(1, 1500, MoveAnimUpdate);
}

void ledConfigTask(void* pvParameters)
{
  bool setup0done = false;
  bool setup1done = false;
  bool setup2done = false;
  bool setup3done = false;
  bool setup4done = false;
  while (true) {
    switch (program) {
      case 0:
        if (!setup0done) {
          Serial.println("Running Setup 0");
          SetRandomSeed();
          for (uint16_t pixel = 0; pixel < PixelCount; pixel++) {
            RgbColor color = RgbColor(random(255), random(255), random(255));
            strip.SetPixelColor(pixel, color);
          }
          setup0done = true;
        }
        if (animations.IsAnimating()) {
          animations.UpdateAnimations();
          strip.Show();
        }
        else
          SetupAnimationSet();
        break;
      case 1:
        if (!setup1done) {
          Serial.println("Running Setup 1");
          SetupAnimations();
          setup0done = false;
          setup1done = true;
        }
        animations.UpdateAnimations();
        strip.Show();
        break;
      case 2:
        if (!setup2done) {
          Serial.println("Running Setup 2");
          SetRandomSeed();
          setup1done = false;
          setup2done = true;
        }
        if (animations.IsAnimating()) {
          animations.UpdateAnimations();
          strip.Show();
        }
        else
          FadeInFadeOutRinseRepeat(0.2f);
        break;
      case 3:
        if (!setup3done) {
          Serial.println("Running Setup 3");
          SetRandomSeed();
          setup2done = false;
          setup3done = true;
          animations.StartAnimation(0, NextPixelMoveDuration, LoopAnimUpdate);
        }
        animations.UpdateAnimations();
        strip.Show();
        break;
      case 4:
        if (!setup4done) {
          Serial.println("Running Setup 4");
          SetRandomSeed();
          setup3done = false;
          setup4done = true;
        }
        if (animations.IsAnimating()) {
          animations.UpdateAnimations();
          strip.Show();
        }
        else {
          PickRandom(0.2f); // 0.0 = black, 0.25 is normal, 0.5 is bright
        }
        break;
    }
    vTaskDelay(1);
  }
}

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    ; // wait for serial attach

  strip.Begin();
  strip.Show();

  // create task to run the animations
  if (xTaskCreate(ledConfigTask, "Led Task", 4096, NULL, 1, &ledTaskHandle) != pdPASS) {
    Serial.println("Failed to create Led Task");
    ESP.restart();
  }
  else
    Serial.println("Led Task Created\n");
}

void loop()
{
  if (digitalRead(BUTTON_PIN) == HIGH) {
    program++;
    while (digitalRead(BUTTON_PIN) == HIGH)
      vTaskDelay(1);
  }

  vTaskDelay(1);
}