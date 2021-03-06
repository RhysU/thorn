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

// Generate a column-major bitmap of the Thorn fractal in *buf
// buf will be realloc-ed as necessary to match width, height.
void thorn(uint8_t **buf, size_t width, size_t height, double cx, double cy,
           double xmin, double xmax, double ymin, double ymax,
           uint8_t maxiter, double escape);

// Write a column-major bitmap to disk in binary PGM
int pgmwrite(const char *name, size_t width, size_t height,
             uint8_t *data, const char *comment);

// Parse comma-separated pairs of floating point values
int scan_double_pair(const char *str, double *a, double *b);
int scan_sizet_pair (const char *str, size_t *a, size_t *b);

int main(int argc, char *argv[])
{
    // Default options
    size_t width   =  1024;
    size_t height  =  512;
    double cx      =  9.984;
    double cy      =  7.55;
    long   maxiter =  255;
    double escape  =  1e4;
    double xmin_pi = -1;
    double xmax_pi =  1;
    double ymin_pi = -1;
    double ymax_pi =  1;

    // Parse command line flags
    int opt;
    int fail = 0;
    while ((opt = getopt(argc, argv, "c:e:m:s:x:y:")) != -1) {
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
            if (maxiter > UINT8_MAX) {
                fprintf(stderr, "Iterations must be less than %d", UINT8_MAX+1);
                fail = 1;
            }
            break;
        case 's':
            if (scan_sizet_pair(optarg, &width, &height)) {
                fprintf(stderr, "Expected a size_t pair \"width,height\"\n");
                fail = 1;
            }
            break;
        case 'x':
            if (scan_double_pair(optarg, &xmin_pi, &xmax_pi)) {
                fprintf(stderr, "Expected a double pair \"xmin/pi,xmax/pi\"\n");
                fail = 1;
            }
            break;
        case 'y':
            if (scan_double_pair(optarg, &ymin_pi, &ymax_pi)) {
                fprintf(stderr, "Expected a double pair \"ymin/pi,ymax/pi\"\n");
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
                                    " [-x xmin/pi,xmax/pi] [-y ymin/pi,ymax/pi]"
                                    " [-m maxiter] [-e escape] pgmfile\n";
        fprintf(stderr, usage, argv[0]);
        exit(EXIT_FAILURE);
    }

    // Generate Thorn fractal for given options
    const double xmin = xmin_pi*M_PI, xmax = xmax_pi*M_PI;
    const double ymin = ymin_pi*M_PI, ymax = ymax_pi*M_PI;
    uint8_t *buf = NULL;
    thorn(&buf, width, height, cx, cy, xmin, xmax, ymin, ymax,
          (uint8_t) maxiter, escape);
    if (!buf) return ENOMEM;

    // Write grayscale buffer to PGM file
    char comment[255];
    snprintf(comment, sizeof(comment), "Thorn fractal: cx=%g, cy=%g, "
             " x=[%g,%g), y=[%g,%g), maxiter=%ld, escape=%g",
             cx, cy, xmin, xmax, ymin, ymax, maxiter, escape);
    pgmwrite(pgmfile, width, height, buf, comment);

    // Free resources and tear down
    free(buf);
    return EXIT_SUCCESS;
}

void thorn(uint8_t **buf, size_t width, size_t height, double cx, double cy,
           double xmin, double xmax, double ymin, double ymax,
           uint8_t maxiter, double escape)
{
    // (*buf) is realloc-ed as necessary to permit use across invocations
    *buf = realloc(*buf, width*height*sizeof(buf[0][0]));
    if (!*buf) return;

    // Notice buf is a private pointer-to-a-pointer so the data /is/ shared.
#pragma omp parallel for default(none)                 \
        firstprivate(buf, width, height, cx, cy, xmin, \
                     xmax, ymin, ymax,maxiter, escape)
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
            (*buf)[i*width + j] = k;
        }
    }
}

int pgmwrite(const char *name, size_t width, size_t height,
             uint8_t *data, const char *comment)
{
    FILE *f = fopen(name, "w");
    if (!f) {
        perror("fopen failed in pgmwrite");
        return 1;
    }

    // Output PGM header per http://netpbm.sourceforge.net/doc/pgm.html
    // Requires one pass through data to compute maximum value
    uint8_t maxval = 0;
    for (size_t i = 0; i < height; ++i)
        for (size_t j = 0; j < width; ++j)
            maxval = data[i*width + j] > maxval ? data[i*width + j] : maxval;
    fprintf(f, "P5\n");
    if (comment) fprintf(f, "# %s\n", comment);
    fprintf(f, "%Zu %Zu\n", width, height);
    fprintf(f, "%"PRIu16"\n", maxval);

    // Output pixels following http://netpbm.sourceforge.net/doc/pgm.html
    // See earlier revisions for code handling uint16_t grayscale values
    for (size_t i = 0; i < height; ++i)
        for (size_t j = 0; j < width; j++)
            fputc((uint8_t) data[i*width + j], f);

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
