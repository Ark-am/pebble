// Works out a time zone's current offset from UTC, with daylight saving, from
// the rules each region follows. The phone's JavaScript may not have a time
// zone database, so this does not rely on one.

// The UTC date of the nth given weekday (0 = Sunday) of a month (0 = January);
// n = -1 means the last one.
function nthWeekday(year, month, weekday, n) {
  if (n > 0) {
    var first = new Date(Date.UTC(year, month, 1)).getUTCDay();
    return 1 + (weekday - first + 7) % 7 + (n - 1) * 7;
  }
  var lastDay = new Date(Date.UTC(year, month + 1, 0)).getUTCDate();
  var last = new Date(Date.UTC(year, month, lastDay)).getUTCDay();
  return lastDay - (last - weekday + 7) % 7;
}

// A moment given as a local time in the zone, with the zone's offset then.
function at(year, month, day, hour, offsetMinutes) {
  return Date.UTC(year, month, day, hour) - offsetMinutes * 60 * 1000;
}

// Each rule returns whether daylight saving is in effect at a UTC moment, for
// a zone with the given standard offset.
var RULES = {
  // United States and Canada: 2:00 on the second Sunday in March until 2:00
  // on the first Sunday in November.
  US: function (now, year, standard) {
    var start = at(year, 2, nthWeekday(year, 2, 0, 2), 2, standard);
    var end = at(year, 10, nthWeekday(year, 10, 0, 1), 2, standard + 60);
    return now >= start && now < end;
  },
  // European Union and the UK: 1:00 UTC on the last Sunday in March until
  // 1:00 UTC on the last Sunday in October.
  EU: function (now, year) {
    var start = Date.UTC(year, 2, nthWeekday(year, 2, 0, -1), 1);
    var end = Date.UTC(year, 9, nthWeekday(year, 9, 0, -1), 1);
    return now >= start && now < end;
  },
  // Egypt: midnight at the start of the last Friday in April until midnight
  // at the end of the last Thursday in October.
  EG: function (now, year, standard) {
    var start = at(year, 3, nthWeekday(year, 3, 5, -1), 0, standard);
    var end = at(year, 9, nthWeekday(year, 9, 4, -1) + 1, 0, standard + 60);
    return now >= start && now < end;
  },
  // South-eastern Australia: 2:00 on the first Sunday in October until 3:00
  // on the first Sunday in April, across the new year.
  AU: function (now, year, standard) {
    var end = at(year, 3, nthWeekday(year, 3, 0, 1), 3, standard + 60);
    var start = at(year, 9, nthWeekday(year, 9, 0, 1), 2, standard);
    return now < end || now >= start;
  },
  // New Zealand: 2:00 on the last Sunday in September until 3:00 on the
  // first Sunday in April.
  NZ: function (now, year, standard) {
    var end = at(year, 3, nthWeekday(year, 3, 0, 1), 3, standard + 60);
    var start = at(year, 8, nthWeekday(year, 8, 0, -1), 2, standard);
    return now < end || now >= start;
  }
};

// Minutes the zone is ahead of UTC at a moment (a Date).
function offsetMinutes(entry, date) {
  var rule = entry.dst && RULES[entry.dst];
  if (!rule) {
    return entry.standard;
  }
  var now = date.getTime();
  // The year where the zone is, near enough for the rules above.
  var year = new Date(now + entry.standard * 60 * 1000).getUTCFullYear();
  return entry.standard + (rule(now, year, entry.standard) ? 60 : 0);
}

module.exports = { offsetMinutes: offsetMinutes };
