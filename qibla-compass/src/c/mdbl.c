#include <pebble.h>

#ifdef PBL_COMPASS
static CompassHeadingData s_compass_data;
static bool s_compass_has_sample;
static bool s_compass_started;

static void compass_heading_handler(CompassHeadingData data) {
  s_compass_data = data;
  s_compass_has_sample = true;
}
#endif

int32_t isDebugBuild(void) {
#ifdef PBL_DEBUG
  return 1;
#else
  return 0;
#endif
}

int32_t compassStart(void) {
#ifdef PBL_COMPASS
  if (s_compass_started) {
    compass_service_unsubscribe();
  }

  s_compass_has_sample = false;
  compass_service_set_heading_filter(TRIG_MAX_ANGLE / 180);
  compass_service_subscribe(compass_heading_handler);
  s_compass_started = true;
  return 1;
#else
  return 0;
#endif
}

int32_t compassReadHeading(void) {
#ifdef PBL_COMPASS
  CompassHeadingData data;

  if (!s_compass_started) {
    return -2;
  }

  // Polling avoids depending on the Alloy compass module's callback bridge,
  // which may not deliver samples on physical watches. Keep the callback too,
  // so the most recent event is available if peek has not produced data yet.
  if (compass_service_peek(&data) != 0) {
    if (!s_compass_has_sample) {
      return -1;
    }
    data = s_compass_data;
  }

  if (data.compass_status == CompassStatusUnavailable) {
    return -2;
  }
  if (data.compass_status == CompassStatusDataInvalid) {
    return -1;
  }

  // Pebble headings increase counter-clockwise. The app's bearings and turn
  // directions use the conventional clockwise-from-north representation.
  int32_t raw_heading = data.magnetic_heading % TRIG_MAX_ANGLE;
  if (raw_heading < 0) {
    raw_heading += TRIG_MAX_ANGLE;
  }
  int32_t counter_clockwise = TRIGANGLE_TO_DEG(raw_heading);
  return (360 - counter_clockwise) % 360;
#else
  return -2;
#endif
}

int main(void) {
  Window *w = window_create();
  window_stack_push(w, true);

  ModdableCreationRecord cr = {
    .recordSize = sizeof(cr),
    .fxBuildFFI = fxBuildFFI,
  };

#ifdef PBL_DEBUG
  // Built with `pebble build --debug`: enable the xsbug JavaScript debugger.
  cr.flags = kModdableCreationFlagDebug;
#endif
  moddable_createMachine(&cr);

#ifdef PBL_COMPASS
  if (s_compass_started) {
    compass_service_unsubscribe();
  }
#endif

  window_destroy(w);
}
