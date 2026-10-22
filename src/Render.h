#pragma once

// Tiny anti-aliased renderer into an RGB565 framebuffer.
//
// Header-only and free of Arduino dependencies, so the exact same drawing
// code runs on the ESP32 and in the host-side preview (tools/preview.cpp).
// Shapes are drawn from signed distances: a pixel's coverage is how far its
// center is inside the edge, which gives smooth edges without supersampling.

#include <math.h>
#include <stdint.h>
#include <string.h>

struct Rgb {
  uint8_t r, g, b;
};

constexpr Rgb rgb(uint32_t hex) {
  return Rgb{static_cast<uint8_t>(hex >> 16), static_cast<uint8_t>(hex >> 8),
             static_cast<uint8_t>(hex)};
}

inline Rgb mix(Rgb a, Rgb b, float t) {
  return Rgb{static_cast<uint8_t>(a.r + (b.r - a.r) * t),
             static_cast<uint8_t>(a.g + (b.g - a.g) * t),
             static_cast<uint8_t>(a.b + (b.b - a.b) * t)};
}

inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

// One glyph of a generated font (see tools/gen_fonts.py). The bitmap is 4-bit
// alpha, two pixels per byte, each row padded to a whole byte. (xoff, yoff) is
// the top-left corner relative to the pen position on the baseline.
struct Glyph {
  uint16_t code;
  uint8_t w, h;
  int8_t xoff, yoff;
  uint8_t advance;
  uint32_t offset;
};

struct Font {
  const Glyph *glyphs;
  uint16_t count;
  const uint8_t *bits;
  uint8_t ascent, descent;
};

enum class Align { Left, Center, Right };

// Decode the next UTF-8 code point and advance `s`. Invalid bytes are skipped.
inline uint32_t nextCodepoint(const char *&s) {
  const uint8_t c = static_cast<uint8_t>(*s++);
  if (c < 0x80) return c;
  int extra = 0;
  uint32_t cp = 0;
  if ((c & 0xE0) == 0xC0) { extra = 1; cp = c & 0x1F; }
  else if ((c & 0xF0) == 0xE0) { extra = 2; cp = c & 0x0F; }
  else if ((c & 0xF8) == 0xF0) { extra = 3; cp = c & 0x07; }
  else return 0;
  for (int i = 0; i < extra; i++) {
    const uint8_t n = static_cast<uint8_t>(*s);
    if ((n & 0xC0) != 0x80) return 0;
    cp = (cp << 6) | (n & 0x3F);
    s++;
  }
  return cp;
}

inline const Glyph *findGlyph(const Font &font, uint32_t code) {
  int lo = 0, hi = font.count - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    const uint16_t c = font.glyphs[mid].code;
    if (c == code) return &font.glyphs[mid];
    if (c < code) lo = mid + 1;
    else hi = mid - 1;
  }
  return nullptr;
}

class Canvas {
public:
  Canvas(uint16_t *pixels, int width, int height)
      : _px(pixels), _w(width), _h(height) {}

  int width() const { return _w; }
  int height() const { return _h; }
  uint16_t *pixels() const { return _px; }

  void clear(Rgb c) {
    // Two pixels per 32-bit store.
    const uint32_t v = pack(c);
    const uint32_t vv = v | (v << 16);
    uint32_t *p = reinterpret_cast<uint32_t *>(_px);
    const int n = _w * _h / 2;
    for (int i = 0; i < n; i++) p[i] = vv;
    if ((_w * _h) & 1) _px[_w * _h - 1] = static_cast<uint16_t>(v);
  }

  // Blend RGB565 color `fg` over a pixel with integer opacity a256 (0..256).
  // Integer math only: this is the inner loop of every shape and glyph.
  static inline void blend565(uint16_t &d, uint16_t fg, int a256) {
    if (a256 >= 256) {
      d = fg;
      return;
    }
    const int bg = d;
    int r = bg >> 11, g = (bg >> 5) & 63, b = bg & 31;
    r += (((fg >> 11) - r) * a256) >> 8;
    g += ((((fg >> 5) & 63) - g) * a256) >> 8;
    b += (((fg & 31) - b) * a256) >> 8;
    d = static_cast<uint16_t>((r << 11) | (g << 5) | b);
  }

  // Blend color `c` over pixel (x, y) with opacity `a` (0..1).
  inline void blend(int x, int y, Rgb c, float a) {
    if (a <= 0.003f || x < 0 || y < 0 || x >= _w || y >= _h) return;
    blend565(_px[y * _w + x], pack(c), a >= 0.997f ? 256 : static_cast<int>(a * 256.0f));
  }

  void fillRect(int x, int y, int w, int h, Rgb c, float a = 1) {
    for (int j = y; j < y + h; j++)
      for (int i = x; i < x + w; i++) blend(i, j, c, a);
  }

  void fillCircle(float cx, float cy, float r, Rgb c, float a = 1) {
    forBox(cx - r, cy - r, cx + r, cy + r, [&](float px, float py) {
      const float dx = px - cx, dy = py - cy;
      return r - sqrtf(dx * dx + dy * dy);
    }, c, a);
  }

  // Soft glow: opacity falls off smoothly from the center to radius r.
  void glow(float cx, float cy, float r, Rgb c, float a) {
    const int x0 = static_cast<int>(cx - r), x1 = static_cast<int>(cx + r) + 1;
    const int y0 = static_cast<int>(cy - r), y1 = static_cast<int>(cy + r) + 1;
    for (int y = y0; y <= y1; y++) {
      for (int x = x0; x <= x1; x++) {
        const float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
        const float t = 1 - sqrtf(dx * dx + dy * dy) / r;
        if (t > 0) blend(x, y, c, a * t * t);
      }
    }
  }

  void ring(float cx, float cy, float r, float thickness, Rgb c, float a = 1) {
    const float ro = r + thickness / 2;
    forBox(cx - ro, cy - ro, cx + ro, cy + ro, [&](float px, float py) {
      const float dx = px - cx, dy = py - cy;
      return thickness / 2 - fabsf(sqrtf(dx * dx + dy * dy) - r);
    }, c, a);
  }

  // Line segment with round caps and radius r.
  void capsule(float x0, float y0, float x1, float y1, float r, Rgb c,
               float a = 1) {
    const float vx = x1 - x0, vy = y1 - y0;
    const float len2 = vx * vx + vy * vy;
    forBox(fminf(x0, x1) - r, fminf(y0, y1) - r, fmaxf(x0, x1) + r,
           fmaxf(y0, y1) + r, [&](float px, float py) {
      const float wx = px - x0, wy = py - y0;
      float t = len2 > 0 ? (wx * vx + wy * vy) / len2 : 0;
      t = clamp01(t);
      const float dx = wx - t * vx, dy = wy - t * vy;
      return r - sqrtf(dx * dx + dy * dy);
    }, c, a);
  }

  void roundRect(float x, float y, float w, float h, float r, Rgb c,
                 float a = 1) {
    roundRectGradient(x, y, w, h, r, c, c, a);
  }

  // Rounded rectangle filled with a vertical gradient from `top` to `bottom`.
  void roundRectGradient(float x, float y, float w, float h, float r, Rgb top,
                         Rgb bottom, float a = 1) {
    const float cx = x + w / 2, cy = y + h / 2;
    const float hx = w / 2 - r, hy = h / 2 - r;
    const int x0 = static_cast<int>(floorf(x)), x1 = static_cast<int>(ceilf(x + w));
    const int y0 = static_cast<int>(floorf(y)), y1 = static_cast<int>(ceilf(y + h));
    // 4x4 ordered dither so slow gradients don't band in RGB565. Offsets
    // are in 1/16 of a quantization step, centered on zero.
    static const int8_t kBayer[16] = {-15, 1, -11, 5, 9, -7, 13, -3,
                                      -9, 7, -13, 3, 15, -1, 11, -5};
    const int a256 = a >= 0.997f ? 256 : static_cast<int>(a * 256.0f);
    for (int py = y0; py < y1; py++) {
      if (py < 0 || py >= _h) continue;
      const float t = clamp01((py + 0.5f - y) / h);
      // Row color in 8.4 fixed point (value * 16).
      const int fr = static_cast<int>((top.r + (bottom.r - top.r) * t) * 16);
      const int fg = static_cast<int>((top.g + (bottom.g - top.g) * t) * 16);
      const int fb = static_cast<int>((top.b + (bottom.b - top.b) * t) * 16);
      const float qy = fmaxf(fabsf(py + 0.5f - cy) - hy, 0.0f);
      // Columns where the row is fully inside (no corner math needed).
      int inL = x1, inR = x0 - 1;
      if (qy <= 0.0f) {
        inL = static_cast<int>(ceilf(cx - hx - 0.5f));
        inR = static_cast<int>(floorf(cx + hx - 0.5f));
      }
      uint16_t *row = _px + py * _w;
      const int8_t *bay = kBayer + (py & 3) * 4;
      for (int px = x0; px < x1; px++) {
        if (px < 0 || px >= _w) continue;
        int cov256 = 256;
        if (px < inL || px > inR) {
          const float qx = fmaxf(fabsf(px + 0.5f - cx) - hx, 0.0f);
          const float cov = clamp01(r - sqrtf(qx * qx + qy * qy) + 0.5f);
          if (cov <= 0) continue;
          cov256 = static_cast<int>(cov * 256.0f);
        }
        // Dither by up to half a quantization step: R/B step is 8 (5 bits),
        // G step is 4 (6 bits); k/32 of a step, in 1/16 units.
        const int k = bay[px & 3];
        int cr = (fr + k * 4) >> 4, cg = (fg + k * 2) >> 4, cb = (fb + k * 4) >> 4;
        cr = cr < 0 ? 0 : (cr > 255 ? 255 : cr);
        cg = cg < 0 ? 0 : (cg > 255 ? 255 : cg);
        cb = cb < 0 ? 0 : (cb > 255 ? 255 : cb);
        const uint16_t c565 =
            static_cast<uint16_t>(((cr & 0xF8) << 8) | ((cg & 0xFC) << 3) | (cb >> 3));
        blend565(row[px], c565, (cov256 * a256) >> 8);
      }
    }
  }

  // Filled polygon (even-odd), 4x4 supersampled for smooth edges.
  void polygon(const float *xy, int n, Rgb c, float a = 1) {
    float minX = xy[0], maxX = xy[0], minY = xy[1], maxY = xy[1];
    for (int i = 1; i < n; i++) {
      minX = fminf(minX, xy[i * 2]); maxX = fmaxf(maxX, xy[i * 2]);
      minY = fminf(minY, xy[i * 2 + 1]); maxY = fmaxf(maxY, xy[i * 2 + 1]);
    }
    for (int y = static_cast<int>(minY); y <= static_cast<int>(maxY); y++) {
      for (int x = static_cast<int>(minX); x <= static_cast<int>(maxX); x++) {
        int hits = 0;
        for (int sy = 0; sy < 4; sy++) {
          for (int sx = 0; sx < 4; sx++) {
            if (insidePolygon(xy, n, x + (sx + 0.5f) / 4, y + (sy + 0.5f) / 4)) hits++;
          }
        }
        if (hits) blend(x, y, c, a * hits / 16.0f);
      }
    }
  }

  // Width in pixels of a UTF-8 string. With `tabular`, digits share the
  // width of the widest digit so a ticking clock doesn't jitter.
  int textWidth(const Font &font, const char *text, bool tabular = false) const {
    int w = 0;
    const char *s = text;
    while (*s) {
      const uint32_t cp = nextCodepoint(s);
      w += advanceOf(font, cp, tabular);
    }
    return w;
  }

  // Draw UTF-8 text with its baseline at `baseline`. Returns the end x.
  int text(const Font &font, int x, int baseline, const char *text, Rgb c,
           Align align = Align::Left, float a = 1, bool tabular = false) {
    if (align != Align::Left) {
      const int w = textWidth(font, text, tabular);
      x -= align == Align::Center ? w / 2 : w;
    }
    const char *s = text;
    while (*s) {
      const uint32_t cp = nextCodepoint(s);
      const Glyph *g = findGlyph(font, cp);
      const int adv = advanceOf(font, cp, tabular);
      if (g) {
        // Center proportional digits inside their tabular cell.
        const int shift = tabular && cp >= '0' && cp <= '9' ? (adv - g->advance) / 2 : 0;
        drawGlyph(font, *g, x + shift, baseline, c, a);
      }
      x += adv;
    }
    return x;
  }

private:
  uint16_t *_px;
  int _w, _h;

  static inline uint16_t pack(Rgb c) {
    return static_cast<uint16_t>(((c.r & 0xF8) << 8) | ((c.g & 0xFC) << 3) | (c.b >> 3));
  }
  static inline Rgb unpack(uint16_t v) {
    const uint8_t r = (v >> 11) & 0x1F, g = (v >> 5) & 0x3F, b = v & 0x1F;
    return Rgb{static_cast<uint8_t>((r << 3) | (r >> 2)),
               static_cast<uint8_t>((g << 2) | (g >> 4)),
               static_cast<uint8_t>((b << 3) | (b >> 2))};
  }

  // Run `dist(px, py)` (positive inside, in pixels) over a bounding box.
  template <typename F>
  void forBox(float x0, float y0, float x1, float y1, F dist, Rgb c, float a) {
    const int ix0 = static_cast<int>(floorf(x0)) - 1, ix1 = static_cast<int>(ceilf(x1)) + 1;
    const int iy0 = static_cast<int>(floorf(y0)) - 1, iy1 = static_cast<int>(ceilf(y1)) + 1;
    for (int y = iy0; y <= iy1; y++) {
      if (y < 0 || y >= _h) continue;
      for (int x = ix0; x <= ix1; x++) {
        const float cov = clamp01(dist(x + 0.5f, y + 0.5f) + 0.5f);
        if (cov > 0) blend(x, y, c, a * cov);
      }
    }
  }

  static bool insidePolygon(const float *xy, int n, float px, float py) {
    bool in = false;
    for (int i = 0, j = n - 1; i < n; j = i++) {
      const float xi = xy[i * 2], yi = xy[i * 2 + 1];
      const float xj = xy[j * 2], yj = xy[j * 2 + 1];
      if ((yi > py) != (yj > py) && px < (xj - xi) * (py - yi) / (yj - yi) + xi) in = !in;
    }
    return in;
  }

  static int advanceOf(const Font &font, uint32_t cp, bool tabular) {
    if (tabular && cp >= '0' && cp <= '9') {
      int widest = 0;
      for (uint32_t d = '0'; d <= '9'; d++) {
        const Glyph *g = findGlyph(font, d);
        if (g && g->advance > widest) widest = g->advance;
      }
      return widest;
    }
    const Glyph *g = findGlyph(font, cp);
    return g ? g->advance : font.ascent / 3;
  }

  void drawGlyph(const Font &font, const Glyph &g, int x, int baseline, Rgb c,
                 float a) {
    const uint8_t *bits = font.bits + g.offset;
    const int rowBytes = (g.w + 1) / 2;
    const uint16_t c565 = pack(c);
    // Opacity per 4-bit coverage level, precomputed once per glyph.
    int alpha[16];
    const int a256 = a >= 0.997f ? 256 : static_cast<int>(a * 256.0f);
    for (int v = 0; v < 16; v++) alpha[v] = (v * a256 + 7) / 15;
    const int gx = x + g.xoff;
    for (int j = 0; j < g.h; j++) {
      const int y = baseline + g.yoff + j;
      if (y < 0 || y >= _h) continue;
      uint16_t *row = _px + y * _w;
      const uint8_t *src = bits + j * rowBytes;
      for (int i = 0; i < g.w; i++) {
        const uint8_t byte = src[i >> 1];
        const uint8_t v = (i & 1) ? (byte & 0x0F) : (byte >> 4);
        const int px = gx + i;
        if (v && px >= 0 && px < _w) blend565(row[px], c565, alpha[v]);
      }
    }
  }
};
