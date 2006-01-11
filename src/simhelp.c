/**
 * C helper file for sim.pyx
 *
 * $Id$
 *
 * CHANGES  (reverse chronological order, use CVS log for details)
 *
 * wware 060111 - Be more careful with error checking in do_python_callback.
 *
 * wware 060109 - Made several changes to facilitate passing Python
 * exceptions upstream from deep inside C function call stacks.
 *
 * wware 060102 - Added a callback for Python to pick up trace file info.
 *
 * WARNING: This file is not compiled separately -- it's #included in sim.c
 * due to the "cdef extern" declaration in sim.pyx which names it.
 * For some reason distutils doesn't realize this means there's a dependency,
 * so Will added one in setup.py to fix this.
 * [bruce 060101]
 *
 */

char __author__[] = "Will";

#include "Python.h"
#include "Numeric/arrayobject.h"
#include "simulator.h"

void initsimhelp(void);
PyObject * readPart(void);
void dumpPart(void);
PyObject *everythingElse(void);
char * structCompareHelp(void);

static char retval[100];
static struct part *part;
static struct xyz *pos;
static char buf[1024];
static int callback_exception = 0;

char *filename;

static void *mostRecentSimObject = NULL;

// Python exception stuff, wware 010609
char *py_exc_str = NULL;
static char py_exc_strbuf[1024];

static void
begin_python_call(void)
{
    py_exc_str = NULL;
    callback_exception = 0;
}

void
reinitSimGlobals(PyObject *sim)
{
    reinit_globals();
    mostRecentSimObject = sim;
}

PyObject *
verifySimObject(PyObject *sim)
{
    begin_python_call();
    if (sim != mostRecentSimObject) {
	PyErr_SetString(PyExc_AssertionError,
			"not the most recent simulator object");
	return NULL;
    }
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *writeTraceCallbackFunc = NULL;
static PyObject *frameCallbackFunc = NULL;

static PyObject *
setCallbackFunc(PyObject *f, PyObject **cb)
{
    if (f == Py_None) {
	*cb = NULL;
	Py_INCREF(Py_None);
	return Py_None;
    } else if (f != NULL && PyCallable_Check(f)) {
	*cb = f;
	Py_INCREF(Py_None);
	return Py_None;
    }
    *cb = NULL;
    PyErr_SetString(PyExc_RuntimeError, "bad callback");
    return NULL;
}

PyObject *
setWriteTraceCallbackFunc(PyObject *f)
{
    begin_python_call();
    return setCallbackFunc(f, &writeTraceCallbackFunc);
}

PyObject *
setFrameCallbackFunc(PyObject *f)
{
    begin_python_call();
    return setCallbackFunc(f, &frameCallbackFunc);
}


// wware 060102   callback for getting info from C to python
static void
do_python_callback(PyObject *callbackFunc, PyObject* args)
{
    PyObject *pValue;
    if (callbackFunc == NULL) return;
    if (PyErr_Occurred()) {
	callback_exception = 1;
	return;
    }
    if (!PyCallable_Check(callbackFunc)) {
	callback_exception = 1;
	PyErr_SetString(PyExc_RuntimeError, "callback not callable");
    }
    pValue = PyObject_CallObject(callbackFunc, args);
    if (PyErr_Occurred()) callback_exception = 1;
    Py_DECREF(args);
    Py_XDECREF(pValue);
}

// wware 060102  callback for trace file
void
write_traceline(const char *format, ...)
{
    va_list args;

    if (writeTraceCallbackFunc != NULL || tracef != NULL) {
        va_start(args, format);
        vsnprintf(buf, 1024, format, args);
        va_end(args);
	if (writeTraceCallbackFunc != NULL)
	    do_python_callback(writeTraceCallbackFunc, Py_BuildValue("(s)", buf));
	if (tracef != NULL)
	    fprintf(tracef, "%s", buf);
    }
}

// wware 060101   callback for getting frame info in pyrex
void
callback_writeFrame(struct part *part1, struct xyz *pos1)
{
    if (part != part1) {
	// assert part is <previous value for part>
	// we haven't seen this yet, but it would be important to know about
	// wware 060109  python exception handling
	set_py_exc_str(__FILE__, __FUNCTION__, "the part has changed");
	return;
    }
    pos = pos1;
    if (frameCallbackFunc != NULL)
	do_python_callback(frameCallbackFunc, PyTuple_New(0));
}

// wware 060101   make frame info available in pyrex
PyObject *
getFrame_c(void)
{
// .xyz files are in angstroms (1e-10 m)
#define XYZ (1.0e-2)
    PyObject *retval;
    double *data;
    int i, n;

    begin_python_call();
    if (part == NULL) {
	PyErr_SetString(PyExc_MemoryError,
			"part is null");
	return NULL;
    }
    if (part->num_atoms == 0) {
	return PyString_FromString("");
    }
    n = 3 * part->num_atoms * sizeof(double);
    data = (double *) malloc(n);
    if (data == NULL) {
	PyErr_SetString(PyExc_MemoryError,
			"out of memory");
	return NULL;
    }
    for (i = 0; i < part->num_atoms; i++) {
	data[i * 3 + 0] = pos[i].x * XYZ;
	data[i * 3 + 1] = pos[i].y * XYZ;
	data[i * 3 + 2] = pos[i].z * XYZ;
    }
    retval = PyString_FromStringAndSize((char*) data, n);
    free(data);
    return retval;
}

int printPotentialEnergy = 0; 
// bruce 060101 made this global from localvar


/*
 * A good goal would be to eliminate all the filename-twiddling in this
 * function, and only set up the bond tables.
 */
void initsimhelp(void) // WARNING: this duplicates some code from simulator.c
{
    char *printPotential = NULL;
    double printPotentialInitial = 1.0;
    double printPotentialIncrement = 1.0;
    double printPotentialLimit = 200.0;
    char *ofilename;
    char *tfilename;
    char *p;

    begin_python_call();
    ofilename = "";
    tfilename = "";

    if (DumpAsText) {
        OutputFormat = 0;
    } else {
        // bruce 060103 part of bugfix for Dynamics output format after Minimize
        // (to complete the fix it would be necessary for every change by sim.pyx of either
        //  OutputFormat or DumpAsText to make sure the other one changed to fit,
        //  either at the time of the change or before the next .go method
        // (or if changed during that method, before their next use by any C code); 
        // this is not needed by the present client code, so I'll put it off for now 
        // and hope we can more extensively clean up this option later.)
        OutputFormat = 1; // sim.pyx only tries to support "old" dpb format for now 
    }
    if (strchr(filename, '.')) {
        sprintf(buf, "%s", filename);
    } else if (baseFilename != NULL && strlen(baseFilename) > 0) {
        sprintf(buf, "%s.xyz", filename);
    } else {
        sprintf(buf, "%s.mmp", filename);
    }
    if (ofilename == NULL || strlen(ofilename) == 0) {
	strcpy(OutFileName,buf);
	p = strchr(OutFileName, '.');
	if (p) {
            *p = '\0';
        }
    } else {
        strcpy(OutFileName,ofilename);
    }
    if (! strchr(OutFileName, '.')) {
	if (DumpAsText || baseFilename != NULL) {
            strcat(OutFileName,".xyz");
        } else {
            strcat(OutFileName,".dpb");
        }
    }
    if (tfilename == NULL || strlen(tfilename) == 0) {
	strcpy(TraceFileName,buf);
	p = strchr(TraceFileName, '.');
	if (p) {
            *p = '\0';
        }
    } else {
        strcpy(TraceFileName,tfilename);
    }
    if (! strchr(TraceFileName, '.')) {
        strcat(TraceFileName,".trc");
    }
    
    // bruce 060101 moved the rest of this function into the start of everythingElse 
    // since it depends on parameters set by the client code after this init method runs,
    // but then had to move initializeBondTable back here to fix a bug (since mmp reading
    // depends on it)
    initializeBondTable();
}

// wware 060109  python exception handling
#define PYBAIL() \
  if (py_exc_str != NULL) { \
    PyErr_SetString(PyExc_RuntimeError, py_exc_str); return NULL; }

PyObject *
readPart(void)
{
    // wware 060109  python exception handling
    begin_python_call();
    part = readMMP(buf);
    if (part == NULL) {
	set_py_exc_str(__FILE__, __FUNCTION__, "part is null");
	PYBAIL();
    }
    updateVanDerWaals(part, NULL, part->positions);
    PYBAIL();
    generateStretches(part);
    PYBAIL();
    generateBends(part);
    PYBAIL();
    Py_INCREF(Py_None);
    return Py_None;
}

void dumpPart(void)
{
    printPart(stdout, part);
}

PyObject *
everythingElse(void) // WARNING: this duplicates some code from simulator.c
{
    // wware 060109  python exception handling
    begin_python_call();
    // bruce 060101 moved this section here, from the end of initsimhelp,
    // since it depends on parameters set by the client code after that init method runs
    if (!printPotentialEnergy) {
        tracef = fopen(TraceFileName, "w");
        if (!tracef) {
            perror(TraceFileName);
            exit(1);
        }
        fprintf(tracef, "# %s\n", "run from pyrex interface"); // like printing the commandLine
        // ##e should print options set before run, but it's too early to do that in this code
    }
    if (IterPerFrame <= 0) IterPerFrame = 1;
    // initializeBondTable(); // this had to be done in initsimhelp instead [bruce 060101]
    // end of section moved by bruce 060101

    traceHeader(tracef, filename, OutFileName, TraceFileName, 
		part, NumFrames, IterPerFrame, Temperature);

    if  (ToMinimize) {
	NumFrames = max(NumFrames,(int)sqrt((double)part->num_atoms));
	Temperature = 0.0;
    } else {
        traceJigHeader(tracef, part);
    }

    // printf("iters per frame = %d\n",IterPerFrame);
    // printf("number of frames = %d\n",NumFrames);
    // printf("timestep = %e\n",Dt);
    // printf("temp = %f\n",Temperature);
    // if (DumpAsText) printf("dump as text\n");

    // printf("< %s  > %s\n", buf, OutFileName);

    outf = fopen(OutFileName, DumpAsText ? "w" : "wb");
    if (outf == NULL) {
	snprintf(buf, 1024, "bad output filename: %s", OutFileName);
	PyErr_SetString(PyExc_IOError, buf);
	return NULL;
    }
    writeOutputHeader(outf, part);

    if  (ToMinimize) {
	minimizeStructure(part);
    }
    else {
        dynamicsMovie(part);
    }

    fclose(outf);
    doneNoExit(0, tracef, "");
    if (tracef != NULL) fclose(tracef);

    if (callback_exception) {
	return NULL;
    } else if (py_exc_str != NULL) {
	// wware 060109  python exception handling
	// If we're Interrupted, it will have happened by now
	PyErr_SetString(PyExc_RuntimeError, py_exc_str);
	return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}


/*
 * Decompose dynamicsMovie into steps callable from Python.
 *
 * Later I'd like to decompose dynamicsMovie_step into still-smaller
 * steps, with one subgoal being to move all the jig calculations
 * entirely into Python.
 */

static struct xyz *_averagePositions;
static struct xyz *_oldPositions;
static struct xyz *_newPositions;
static struct xyz *_positions;
static struct xyz *_force;
static int _framenumber;

void
dynamicsMovie_start(void)
{
    int i;

    _averagePositions = (struct xyz *)allocate(sizeof(struct xyz) * part->num_atoms);
    _oldPositions = (struct xyz *)allocate(sizeof(struct xyz) * part->num_atoms);
    _newPositions = (struct xyz *)allocate(sizeof(struct xyz) * part->num_atoms);
    _positions =  (struct xyz *)allocate(sizeof(struct xyz) * part->num_atoms);
    _force = (struct xyz *)allocate(sizeof(struct xyz) * part->num_atoms);

    for (i=0; i<part->num_atoms; i++) {
	vset(_positions[i], part->positions[i]);
	vsub2(_oldPositions[i], _positions[i], part->velocities[i]);
    }
    _framenumber = 0;
    initializeDeltaBuffers(part);
}

void
dynamicsMovie_step(void)
{
    oneDynamicsFrame(part, IterPerFrame,
		     _averagePositions, &_oldPositions, &_newPositions, &_positions, _force);
    writeDynamicsMovieFrame(outf, _framenumber++, part, _averagePositions);
}


void
dynamicsMovie_finish(void)
{
    writeOutputTrailer(outf, part, NumFrames);
    free(_averagePositions);
    free(_oldPositions);
    free(_newPositions);
    free(_positions);
    free(_force);
    doneNoExit(0, tracef, "");
}


/**
 * If we return a non-empty string, it's an error message.
 */
char * structCompareHelp(void) {
    int i1;
    int i2;
    struct xyz *basePositions;
    struct xyz *initialPositions;
        
    if (baseFilename == NULL || strlen(baseFilename) == 0) {
	sprintf(retval, "No baseFilename");
	return retval;
    }
    basePositions = readXYZ(baseFilename, &i1);
    if (basePositions == NULL) {
	sprintf(retval,
		"could not read base positions file from \"%s\"",
		baseFilename);
	return retval;
    }
    initialPositions = readXYZ(filename, &i2);
    if (initialPositions == NULL) {
	sprintf(retval,
		"could not read comparison positions file \"%s\"",
		filename);
	return retval;
    }
    if (i1 != i2) {
	sprintf(retval,
		"structures to compare must have same number of atoms");
	return retval;
    }
    if (doStructureCompare(i1, basePositions, initialPositions,
			   NumFrames, 1e-8, 1e-4, 1.0+1e-4)) {
	sprintf(retval, "structure comparison failed");
	return retval;
    }
    retval[0] = '\0';
    return retval;
}

void
set_interrupted_flag(int value)
{
    // used in sim.pyx by __setattr__, which has already set Interrupted
    // so our only job here is to set py_exc_str
    if (py_exc_str != NULL) return;
    sprintf(py_exc_strbuf, "simulator has been interrupted");
    py_exc_str = py_exc_strbuf;
}

void
set_py_exc_str(const char *filename, const char *funcname,
               const char *format, ...)
{
    va_list args;
    int n;
    if (py_exc_str != NULL) return;
    n = sprintf(py_exc_strbuf, "%s(%s) ", filename, funcname);
    va_start(args, format);
    vsnprintf(py_exc_strbuf + n, 1024 - n, format, args);
    va_end(args);
    py_exc_str = py_exc_strbuf;
}

/*
 * Local Variables:
 * c-basic-offset: 4
 * tab-width: 8
 * End:
 */
