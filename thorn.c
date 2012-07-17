// C99/POSIX code to generate the Thorn fractal in binary PGM format
//
// Based on material at http://paulbourke.net/fractals/thorn/,
// especially http://paulbourke.net/fractals/thorn/thorn_code.c
//
// Build with "cc -O3 -std=c99 -lm thorn.c -o thorn"

// C99 drops getopt from unistd.h without this definition
#define _POSIX_C_SOURCE 200112L

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// C99 dropped M_PI though not all compilers conform
#ifndef M_PI
#define M_PI (3.14159265358979323846264338327)
#endif

// Generate an column-major bitmap of the fractal
uint16_t* thorn(size_t width, size_t height, double cx, double cy,
                uint16_t maxiter, double escape);

// Write a column-major bitmap to disk in binary PGM
int pgmwrite(const char *name, size_t width, size_t height,
             uint16_t *data, const char *comment);

// Parse comma-separated pairs of floating point values
int scan_double_pair(const char *str, double *a, double *b);
int scan_sizet_pair (const char *str, size_t *a, size_t *b);

int main(int argc, char *argv[])
{

    // Default options
    size_t   width   = 1024;
    size_t   height  = 768;
    double   cx      = 9.984;
    double   cy      = 7.55;
    uint16_t maxiter = 1024;
    double   escape  = 1e4;

    // Parse command line flags
    int opt;
    int fail = 0;
    while ((opt = getopt(argc, argv, "c:ems:")) != -1) {
        switch (opt) {
        case 'c':
            if (scan_double_pair(optarg, &cx, &cy)) {
                fprintf(stderr, "Expected a double pair \"cx,cy\"\n");
                fail = 1;
            }
            break;
        case 'e':
            escape  = atof(optarg);
            break;
        case 'm':
            maxiter = atol(optarg);
            break;
        case 's':
            if (scan_sizet_pair(optarg, &width, &height)) {
                fprintf(stderr, "Expected a size_t pair \"width,height\"\n");
                fail = 1;
            }
            break;
        default:
            fail = 1;
            break;
        }
    }
    // Parse command line arguments
    if (optind >= argc) {
        fprintf(stderr, "Expected pgmfile argument after options\n");
        fail = 1;
    }
    const char * const pgmfile = argv[optind];
    // Bail if either failed
    if (fail) {
        static const char usage[] = "Usage: %s [-s width,height] [-c cx,cy]"
                                    " [-m maxiter] [-e escape] pgmfile\n";
        fprintf(stderr, usage, argv[0]);
        exit(EXIT_FAILURE);
    }

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
    const double xmin = -M_PI, xmax = M_PI;
    const double ymin = -M_PI, ymax = M_PI;

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

int scan_double_pair(const char *str, double *a, double *b)
{
    char ignore = '\0';
    return 2 != sscanf(str ? str : "", "%lf , %lf %c", a, b, &ignore);
}

int scan_sizet_pair(const char *str, size_t *a, size_t *b)
{
    char ignore = '\0';
    return 2 != sscanf(str ? str : "", "%zd , %zd %c", a, b, &ignore);
}
