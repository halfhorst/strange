#include <math.h>
#include <stdio.h>
#include <stdbool.h>

struct Vec3 {
  float x;
  float y;
  float z;
};

void cube_init(int w, int h) {}
bool cube_update(char *buffer, int w, int h, int t);
void cube_cleanup(void) {}

float sphereSDF(struct Vec3 point, struct Vec3 center, float radius);
float boxSDF(struct Vec3 p, struct Vec3 c, struct Vec3 b);
float sceneSDF(struct Vec3 point);

struct Vec3 make_vector(float x, float y, float z);
float vector_length(struct Vec3 v);
struct Vec3 normalize(struct Vec3 v);
struct Vec3 vector_sub(struct Vec3 a, struct Vec3 b);
struct Vec3 vector_add(struct Vec3 a, struct Vec3 b);
struct Vec3 vector_mul(struct Vec3 v, float scalar);
struct Vec3 vector_max(struct Vec3 a, float cmp);
struct Vec3 vector_abs(struct Vec3 a);
struct Vec3 estimate_normal(struct Vec3 p);
bool march(struct Vec3 eye, struct Vec3 direction, struct Vec3 *normal);


float sphereSDF(struct Vec3 point, struct Vec3 center, float radius) {
  return vector_length(make_vector(point.x - center.x,
                                   point.y - center.y,
                                   point.z - center.z)) - radius;
}

// float sdBox( vec3 p, vec3 b )
// {
//   vec3 q = abs(p) - b;
//   return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);
// }

// https://www.alanzucconi.com/2016/07/01/signed-distance-functions/#part3
float boxSDF(struct Vec3 p, struct Vec3 c, struct Vec3 b) {

  // rigid body translation
  struct Vec3 translated_p = vector_sub(p, c);
  // rotation

  struct Vec3 q = vector_sub(vector_abs(translated_p), b);
  return vector_length(vector_max(q, 0.0)) + fmin(fmaxf(fmaxf(q.x, q.y), q.z), 0.0);
}


float sceneSDF(struct Vec3 point) {
  // struct Vec3 sphere_center = make_vector(0.0, 0.0, -1.0);
  // float sphere_radius = 0.75;
  // return sphereSDF(point, sphere_center, sphere_radius);

  struct Vec3 box_center = make_vector(0, 0, -1.0);
  struct Vec3 box_first_quadrant = make_vector(0.3, 0.25, 0.1);
  return boxSDF(point, box_center, box_first_quadrant);
}

struct Vec3 make_vector(float x, float y, float z) {
  struct Vec3 made = {.x = x, .y = y, .z = z};
  return made;
}

float vector_length(struct Vec3 v) {
  return sqrt(pow(v.x, 2) + pow(v.y, 2) + pow(v.z, 2));
}

struct Vec3 normalize(struct  Vec3 v) {
  float length = vector_length(v);
  return make_vector(v.x / length, v.y / length, v.z / length);
}

struct Vec3 vector_sub(struct Vec3 a, struct Vec3 b) {
  return make_vector(a.x - b.x, a.y - b.y, a.z - b.z);
}

struct Vec3 vector_add(struct Vec3 a, struct Vec3 b) {
  return make_vector(a.x + b.x, a.y + b.y, a.z + b.z);
}

struct Vec3 vector_mul(struct Vec3 a, float scalar) {
  return make_vector(a.x * scalar, a.y * scalar, a.z * scalar);
}

struct Vec3 vector_max(struct Vec3 a, float cmp) {
  return make_vector(fmaxf(a.x, cmp), fmaxf(a.y, cmp), fmaxf(a.z, cmp));
}

struct Vec3 vector_abs(struct Vec3 a) {
  return make_vector(abs(a.x), abs(a.y), abs(a.z));
}

struct Vec3 estimate_normal(struct Vec3 p) {
  float epsilon = 0.001;
  struct Vec3 estimated = {
    .x = sceneSDF(make_vector(p.x + epsilon, p.y, p.z)) - sceneSDF(make_vector(p.x - epsilon, p.y, p.z)),
    .y = sceneSDF(make_vector(p.x, p.y + epsilon, p.z)) - sceneSDF(make_vector(p.x, p.y - epsilon, p.z)),
    .z = sceneSDF(make_vector(p.x, p.y, p.z + epsilon)) - sceneSDF(make_vector(p.x, p.y, p.z - epsilon))
  };
  return normalize(estimated);
}

bool march(struct Vec3 eye, struct Vec3 direction, struct Vec3 *normal) {
  int min_distance = 0.0;
  int max_distance = 4.0;
  int march_max = 100;
  float epsilon = 0.001;

  float depth = min_distance;
  for (int i = 0; i < march_max; i++) {
    struct Vec3 point = vector_add(eye, vector_mul(direction, depth));
    float distance = sceneSDF(point);
    if (distance < epsilon) {
      *normal = estimate_normal(point);
      return true;
    } else if (distance > max_distance) {
      return false;
    }
    depth += distance;
  }
  return false;
}

// relatively verbose
bool cube_update(char *buffer, int w, int h, int t) {

  // we will use internal relative units for the viewport,
  // then scale to w and h
  float aspect_ratio = 16.0 / 9.0;
  float viewport_height = 4.0;
  float viewport_width = aspect_ratio * viewport_height;

  // setup the viewing planes
  struct Vec3 origin = make_vector(0.0, 0.0, 0.0);  // also camera
  struct Vec3 horizontal_span = make_vector(viewport_width, 0.0, 0.0);
  struct Vec3 vertical_span = make_vector(0.0, viewport_height, 0.0);

  // viewing plane is at z = -1
  struct Vec3 to_viewport_lower_left = vector_sub(vector_sub(vector_sub(origin, vector_mul(horizontal_span, 0.5)), vector_mul(vertical_span, 0.5)), make_vector(0.0, 0.0, 1.0));

  for (int i = 0; i < w; i++) {
    for (int j = 0; j < h; j++) {
      double u = (double) i / (w - 1);
      double v = (double) j / (h - 1);

      // cast a ray from the camera (@ origin) through the pixel
      struct Vec3 ray_direction = vector_sub(vector_add(to_viewport_lower_left, vector_add(vector_mul(horizontal_span, u), vector_mul(vertical_span, v))), origin);

      buffer[(j * w) + i] = ' ';  // background
      struct Vec3 normal;
      if (!march(origin, ray_direction, &normal)) continue;

      // shift normal to [0, 1]
      normal = vector_mul(vector_add(normal, make_vector(1.0, 1.0, 1.0)), 0.5);
      // make_vector((normal.x + 1.) / 2.,
      //                      (normal.y + 1.) / 2.,
      //                      (normal.z + 1.) / 2.);

      char* shades = ".:-=+*#%@";  // from http://paulbourke.net/dataformats/asciiart/
      // printf("%f ", normal.x);

      // we hit, so shade the point
      // we convert the normal to a value in [0, 1] and use it to index the shades
      // higher index is more "dense." I want things pointing at the viewer to be
      // less dense

      float n_float = (0.33 * normal.x) + (0.33 * normal.y) + (0.33 * normal.z);
      // fprintf(stderr, "%f %f %f\n", normal.x, normal.y, normal.z);
      // char *shade = shades[(int) (n_float * 9)];
      buffer[(j * w) + i] = shades[(int) (n_float * 9)];
    }
  }
  return true;
}
