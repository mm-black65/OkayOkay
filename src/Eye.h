#ifndef EYE_H
#define EYE_H

struct Eye {
  float x, y, w, h;                 // current position/size
  float targetX, targetY, targetW, targetH;
  float pupilX, pupilY;             // current pupil offset from eye center
  float targetPupilX, targetPupilY;

  bool blinking = false;
  unsigned long lastBlink = 0;
  unsigned long nextBlinkTime = 0;

  void update() {
    const float lerpSpeed = 0.35f;
    x += (targetX - x) * lerpSpeed;
    y += (targetY - y) * lerpSpeed;
    w += (targetW - w) * lerpSpeed;
    h += (targetH - h) * lerpSpeed;
    pupilX += (targetPupilX - pupilX) * lerpSpeed;
    pupilY += (targetPupilY - pupilY) * lerpSpeed;
  }
};

#endif