#include <iostream>
#include <cmath>
#include "CImg.h"
#include <vector>

/*
* clang++ main.cpp -I /usr/X11R6/include -L/usr/X11R6/lib -lm -lpthread -lX11 -std=c++11 && ./a.out
*/

#include "vec3.h"
#include "Matrix33.h"
#include "Ray.h"
#include "Material.h"

using namespace std;


class Intersection
{
    public:
        vec3 normal, pos;
        double distance;
        Material * material;
        Intersection() :
            distance(-1), normal(vec3()), pos(vec3()), material() { }
        Intersection(double d, vec3 pos_, vec3 normal_, Material * mat_) :
            distance(d), normal(normal_), pos(pos_), material(mat_) { }
};

class Shape
{
    public:
        virtual Intersection intersect(Ray) const = 0;
};

class Sphere : public Shape
{
    public:
        vec3 center;
        double radius;
        Material *material;
        Sphere(vec3 pos, double r, Material *mat_) :
            center(pos), radius(r), material(mat_) { }

        //https://en.wikipedia.org/wiki/Line%E2%80%93sphere_intersection
        Intersection intersect(Ray ray) const {
            vec3 diff = ray.origin - center;
            double a = dot(ray.dir, ray.dir);
            double b = 2 * dot(diff, ray.dir);
            double c = dot(diff, diff) - (radius * radius);

            double d = b * b - 4 * a * c;
            if(d < 0) {
                return Intersection();
            }

            d = sqrt(d);

            double q = 0.5 * (b < 0 ? (-b - d) : (-b + d));
            double r1 = q / a;
            double r2 = c / q;

            if(r1 < 0 && r2 < 0) return Intersection();

            double distance;
            if (r2 < 0) {
                distance = r1;
            } else if (r1 < 0 || r2 < r1) {
                distance = r2;
            } else {
                distance = r1;
            }

            vec3 intersection = ray.origin + ray.dir * distance;
            vec3 normal = intersection - this->center;
            normal.normalize();

            return Intersection(distance, intersection, normal, this->material);
        }
};

class Light {
    public:
        vec3 color, pos;
        Light(vec3 p, vec3 col) : pos(p), color(col) { }
};

class Camera {
    public:
        int width, height;
        double scale;
        vec3 pos;
        Matrix33 rot;

        Camera(double fovy_, int width_, int height_) :
               scale(tan(fovy_)), width(width_), height(height_) {
        }

        //Ported for Josh's camera code
        void lookAt(vec3 target, double rho, double phi, double theta) {
            vec3 yStd(0.0, 1.0, 0.0), zStd(0.0, 0.0, 1.0);
            vec3 z = vec3::Spherical(1.0, phi, theta);
            vec3 y = vec3::Spherical(1.0, M_PI / 2.0 - phi, theta + M_PI);

            this->rot = Matrix33::BasisRotation(yStd, zStd, y, z);
            this->pos = z * rho + target;
        }

        Ray getRay(double screen_x, double screen_y) {
            double x = (2 * screen_x / (double) width - 1) * scale;
            double y = (1 - 2 * screen_y / (double) height) * scale;

            vec3 dir = this->rot * vec3(x, y, -1);
            dir.normalize();

            return Ray(this->pos, dir);
        }
};


void findIntersect(const Ray &ray, vector<Shape *> &shapes, Intersection *closest) {
    Intersection i;
    for(int k = 0; k < shapes.size(); k++) {
        i = shapes[k]->intersect(ray);
        if(i.distance > 0 &&
            (closest->distance < 0 || closest->distance > i.distance)) {
                *closest = i;
        }
    }
}

vec3 reflectAbout(vec3 incoming, vec3 axis) {
    return 2 * dot(axis, incoming) * axis - incoming;
}

// TODO: BUNDLE SHAPES AND LIGHTS AND CAM POS

//Uses the Phong Reflection Model
vec3 localLighting(Intersection intersect, const vec3 &camPos,
                   vector<Shape *> &shapes,
                   vector<Light *> &lights) {

    // TODO: MOVE MATERIAL PROPERTIES INSIDE CLASS
    Intersection shadowIntersection;

    vec3 camDir = camPos - intersect.pos, color;
    camDir.normalize();

    for(int i = 0; i < lights.size(); i++) {
        vec3 lightDir = lights[i]->pos - intersect.pos;
        lightDir.normalize();

        if(dot(intersect.normal, lightDir) < 0)
            continue;

        Ray shadowRay (intersect.pos + lightDir * .0001, lightDir);
        findIntersect(shadowRay, shapes, &shadowIntersection);

        if (shadowIntersection.distance > 0)
            continue;

        vec3 reflected = reflectAbout(lightDir, intersect.normal);
        reflected.normalize();

        //Diffuse
        color += intersect.material->getColor() *
                 intersect.material->kd *
                 dot(lightDir, intersect.normal);

        // Specular
        color += lights[i]->color *
                 intersect.material->ks *
                 pow(dot(camDir, reflected), 64.0);
    }

    return vec3(
        color.x > .1 ? color.x : .1,
        color.y > .1 ? color.y : .1,
        color.z > .1 ? color.z : .1
    );
}

#define MAX_RAY_DEPTH 8

vec3 trace(const Ray &ray, const vec3 &cameraPos,
           vector<Shape *> &shapes,
           vector<Light *> &lights, int depth) {

    if(depth >= MAX_RAY_DEPTH) return vec3(0.0);

    Intersection intersection;
    findIntersect(ray, shapes, &intersection);

    if(intersection.distance < 0)
        return vec3(0.0);

    // Get the reflected ray and calculate color
    vec3 reflDir = reflectAbout(-ray.dir, intersection.normal);
    reflDir.normalize();

    Ray reflRay (intersection.pos + reflDir * .001, reflDir);
    vec3 reflColor = trace(reflRay, cameraPos, shapes, lights, depth + 1);

    // Find the local color (specular, diffuse, ambient)
    vec3 localColor = localLighting(intersection, cameraPos, shapes, lights);
    vec3 color =  localColor + reflColor * intersection.material->kr;

    return color;
}

// TODO: ADD COMMAND LINE OPTIONS
int main() {
    const int WIDTH = 512;
    const int HEIGHT = 512;

    cout << vec3::Spherical(1.0, 0.0, 0.0) << endl;

    cimg_library::CImg<double> img(WIDTH, HEIGHT, 1, 3);
    img.fill(0.0);

    ColorMaterial material1(vec3(0.6, 0.3, 0.3), 0.8, 1.0, 1.0);
    Sphere sphere1(vec3(0.0, 0.0, 0.0), 1.0, &material1);

    ColorMaterial material2(vec3(0.0, 1.0, 0.0), 0.0, 1.0, 1.0);
    Sphere sphere2(vec3(-1.0, 1.0, 0.0), .25, &material2);

    ColorMaterial material3(vec3(1.0, 0.0, 0.0), 0.3, 1.0, 1.0);
    Sphere sphere3(vec3(1.0, -0.5, 0.0), .25, &material3);

    ColorMaterial material4(vec3(0.8, 0.2, 1.0), 0.8, 1.0, 1.0);
    Sphere sphere4(vec3(.75, 2.0, 1.0), .66, &material4);

    vector<Shape *> shapes(4);
    shapes[0] = &sphere1;
    shapes[1] = &sphere2;
    shapes[2] = &sphere3;
    shapes[3] = &sphere4;

    Light light1(vec3(0.0, 6.0, 2.0), vec3(1.0));

    vector<Light *> lights(1);
    lights[0] = &light1;

    vec3 target = vec3(0.0, 0.0, 0.0);

    Camera cam(M_PI/15, WIDTH, HEIGHT);
    cam.lookAt(target, 10.0, M_PI/4.0, M_PI/4.0);

    //Generate the initial rays. One for each pixel in the screen.
    for(int i = 0; i < WIDTH; i++) {
        for(int j = 0; j < HEIGHT; j++) {

            // TODO: PUT ALL OF THIS IN A SAMPLE FUNCTION
            vec3 c (0.0);
            double GRID_SIZE = 3;
            for(int k = 0; k < GRID_SIZE; k++) {
                for(int l = 0; l < GRID_SIZE; l++) {
                    double y_off = (0.5/GRID_SIZE) + (l/GRID_SIZE);
                    double x_off = (0.5/GRID_SIZE) + (k/GRID_SIZE);

                    Ray ray = cam.getRay(i + x_off, j + y_off);

                    vec3 col = trace(ray, cam.pos, shapes, lights, 0);
                    c += col;
                }
            }

            c = c / (double)(GRID_SIZE * GRID_SIZE);

            double colors[3] = { c.x, c.y, c.z };
            img.draw_point(i, j, colors);
        }
    }

    img.normalize(0, 255);
    img.save("rotate5.png");
}