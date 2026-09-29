import Message from "pebble/message";

const LOCATION_KEY = 15026;

// A retryable version of the Alloy Location sensor. The SDK Location class is
// intentionally one-shot and its close() method tries to write immediately.
// That write throws when PebbleKit JS is disconnected, which is exactly when
// this app needs to keep a request pending for a later retry.
class RetryLocation {
  #message;
  #onSample;
  #onError;
  #sample;
  #pendingValues = [];
  #writable = false;

  constructor(options) {
    this.#onSample = options.onSample;
    this.#onError = options.onError;

    this.#message = new Message({
      keys: new Map([["LOCATION", LOCATION_KEY]]),
      target: this,
      onReadable() {
        this.target.#read(this.read());
      },
      onWritable() {
        this.target.#writable = true;
        this.target.#flush();
      }
    });
  }

  request(options = {}) {
    // The phone proxy ignores a request while it believes an earlier watch is
    // still running, which happens when a previous launch exited before its
    // stop was delivered. Stopping first makes every request start fresh.
    //
    // The proxy reads any non-empty accuracy field as true (even "0"), so
    // low accuracy is requested by leaving the field empty.
    this.#pendingValues = [
      "0",
      [
        "1",
        options.enableHighAccuracy ? "1" : "",
        options.timeout ?? "",
        options.maximumAge ?? ""
      ].join(",")
    ];
    this.#flush();
  }

  stop() {
    this.#pendingValues = ["0"];
    this.#flush();
  }

  sample() {
    const sample = this.#sample;
    this.#sample = undefined;
    return sample;
  }

  #flush() {
    if (!this.#writable || !this.#pendingValues.length) {
      return;
    }

    // Send one value at a time; onWritable flushes the next.
    this.#writable = false;
    this.#message.write(new Map([["LOCATION", this.#pendingValues.shift()]]));
  }

  #read(message) {
    const value = message.get("LOCATION");
    if (typeof value !== "string") {
      return;
    }

    const fields = value.split(",");
    if (!parseInt(fields[0])) {
      this.#onError?.call(this, new Error("get location failed"));
      return;
    }

    const sample = this.#sample = {
      latitude: parseFloat(fields[1]),
      longitude: parseFloat(fields[2])
    };

    if (fields[3] !== "") sample.altitude = parseFloat(fields[3]);
    if (fields[4] !== "") sample.accuracy = parseFloat(fields[4]);
    if (fields[5] !== "") sample.altitudeAccuracy = parseFloat(fields[5]);
    if (fields[6] !== "") sample.heading = parseFloat(fields[6]);
    if (fields[7] !== "") sample.speed = parseFloat(fields[7]);
    if (fields[8] !== "") sample.timestamp = parseFloat(fields[8]);

    this.#onSample?.call(this);
  }
}

export default RetryLocation;
