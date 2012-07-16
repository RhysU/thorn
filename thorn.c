// C99/POSIX code to generate the Thorn fractal in binary PGM format
//
// Based on material at http://paulbourke.net/fractals/thorn/,
// especially http://paulbourke.net/fractals/thorn/thorn_code.c
//
// Build with "cc -O3 -std=c99 -lm thorn.c -o thorn"

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// No idea why, but unistd.h isn't pulling in getopt...
int getopt(int argc, char * const argv[], const char *optstring);
extern char *optarg;
extern int optind, opterr, optopt;

// Generate an column-major bitmap of the fractal
uint16_t* thorn(size_t width, size_t height, double cx, double cy,
                uint16_t maxiter, double escape);

// Write a column-major bitmap to disk in binary PGM
int pgmwrite(const char *name, size_t width, size_t height,
             uint16_t *data, const char *comment);

int main(int argc, char *argv[])
{
    // Default options
    size_t   width   = 1024;
    size_t   height  = 768;
    double   cx      = 9.984;
    double   cy      = 7.55;
    uint16_t maxiter = 1024;
    double   escape  = 1e4;

    // Parse command line arguments
    static const char usage[] = "Usage: %s [-w width] [-h height]"
                                " [-x cx] [-y cy] [-m maxiter]"
                                " [-e escape] pgmfile\n";
    int opt;
    while ((opt = getopt(argc, argv, "w:h:x:y:m:e:")) != -1) {
        switch (opt) {
        case 'w': width   = atol(optarg);          break;
        case 'h': height  = atol(optarg);          break;
        case 'x': cx      = atof(optarg);          break;
        case 'y': cy      = atof(optarg);          break;
        case 'm': maxiter = atol(optarg);          break;
        case 'e': escape  = atof(optarg);          break;
        default:  fprintf(stderr, usage, argv[0]); exit(EXIT_FAILURE);
        }
    }
    if (optind >= argc) {
        fprintf(stderr, "Expected pgmfile argument after options\n");
        fprintf(stderr, usage, argv[0]);
        exit(EXIT_FAILURE);
    }
    const char * const pgmfile = argv[optind];

    // Generate Thorn fractal for given options
    uint16_t *data = thorn(width, height, cx, cy, maxiter, escape);
    if (!data) return EXIT_FAILURE;

    // Write grayscale buffer to PGM file
    char comment[255];
    snprintf(comment, sizeof(comment),
             "Thorn fractal: cx=%g, cy=%g, maxiter=%d, escape=%g",
             cx, cy, maxiter, escape);
    pgmwrite(pgmfile, width, height, data, comment);

    // Tear down
    free(data);
    return EXIT_SUCCESS;
}

uint16_t* thorn(size_t width, size_t height, double cx, double cy,
                uint16_t maxiter, double escape)
{
    const double pi = 3.14159265358979323846264338327; // No M_PI in C99?!
    const double xmin = -pi, xmax = pi;
    const double ymin = -pi, ymax = pi;

    uint16_t* data = (uint16_t*) malloc(width*height*sizeof(uint16_t));

    if (data) {
#pragma omp parallel for default(none) shared(data)                           \
                         firstprivate(width, height, cx, cy, maxiter, escape)
        for (size_t i = 0; i < height; i++) {
            const double zi = ymin + i*(ymax - ymin) / height;
            for (size_t j = 0; j < width; j++) {
                const double zr = xmin + j*(xmax - xmin) / width;

                size_t k = 0;
                double ir = zr, ii = zi, a, b;
                do {
                    a = ir;
                    b = ii;
                    ir = a / cos(b) + cx;
                    ii = b / sin(a) + cy;
                } while (k++ < maxiter && (ir*ir + ii*ii) < escape);
                data[i*width + j] = k;
            }
        }
    }

    return data;
}

int pgmwrite(const char *name, size_t width, size_t height,
             uint16_t *data, const char *comment)
{
    FILE *f = fopen(name, "w");
    if (!f) {
        perror("fopen failed in pgmwrite");
        return 1;
    }

    // Output PGM header per http://netpbm.sourceforge.net/doc/pgm.html
    // Requires one pass through data to compute maximum value
    uint16_t maxval = 0;
    for (size_t i = 0; i < height; ++i)
        for (size_t j = 0; j < width; ++j)
            maxval = data[i*width + j] > maxval ? data[i*width + j] : maxval;
    fprintf(f, "P5\n");
    if (comment) fprintf(f, "# %s\n", comment);
    fprintf(f, "%Zu %Zu\n", width, height);
    fprintf(f, "%"PRIu16"\n", maxval);

    // Use 8 bits per pixel when possible, otherwise use 16 bits MSB
    // first again following http://netpbm.sourceforge.net/doc/pgm.html
    if (maxval <= UINT8_MAX) {
        for (size_t i = 0; i < height; ++i)
            for (size_t j = 0; j < width; j++)
                fputc((uint8_t) data[i*width + j], f);
    } else {
        for (size_t i = 0; i < height; ++i) {
            for (size_t j = 0; j < width; ++j) {
                uint16_t val = data[i*width + j];
                uint8_t  msb = val / UINT8_MAX;
                uint8_t  lsb = val & UINT8_MAX;
                fputc(msb, f);
                fputc(lsb, f);
            }
        }
    }

    fclose(f);
    return 0;
}

