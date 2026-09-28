/* FFI GENERATED FILE; DO NOT EDIT! */

#include "xsffi.h"

txAPI* XS = NULL;

extern int32_t isDebugBuild();
extern int32_t compassStart();
extern int32_t compassReadHeading();

static void xs_isDebugBuild(txMachine* the) {
	int32_t result = isDebugBuild();
	XS->fromInteger(the, mxResult, (txInteger)result);
}

static void xs_compassStart(txMachine* the) {
	int32_t result = compassStart();
	XS->fromInteger(the, mxResult, (txInteger)result);
}

static void xs_compassReadHeading(txMachine* the) {
	int32_t result = compassReadHeading();
	XS->fromInteger(the, mxResult, (txInteger)result);
}

void fxBuildFFI(txMachine* the, txAPI* api) {
	XS = api;
	XS->newHostFunction(the, xs_isDebugBuild, 0, 0, 0);
	XS->push(the, mxThis);
	XS->defineID(the, XS->id(the, "isDebugBuild"), 0, 0x0E);
	XS->pop(the);
	XS->newHostFunction(the, xs_compassStart, 0, 0, 0);
	XS->push(the, mxThis);
	XS->defineID(the, XS->id(the, "compassStart"), 0, 0x0E);
	XS->pop(the);
	XS->newHostFunction(the, xs_compassReadHeading, 0, 0, 0);
	XS->push(the, mxThis);
	XS->defineID(the, XS->id(the, "compassReadHeading"), 0, 0x0E);
	XS->pop(the);
}
