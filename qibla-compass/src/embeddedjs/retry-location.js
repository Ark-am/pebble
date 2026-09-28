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
  #options = {};
  #pendingCommand;
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

  request(options) {
    this.#options = options ?? {};
    this.#pendingCommand = "request";
    this.#flush();
  }

  stop() {
    this.#pendingCommand = "stop";
    this.#flush();
  }

  sample() {
    const sample = this.#sample;
    this.#sample = undefined;
    return sample;
  }

  #flush() {
    if (!this.#writable || !this.#pendingCommand) {
      return;
    }

    let value;
    if (this.#pendingCommand === "stop") {
      value = "0";
    }
    else {
      const options = this.#options;
      value = [
        "1",
        options.enableHighAccuracy === undefined
          ? ""
          : Number(options.enableHighAccuracy),
        options.timeout ?? "",
        options.maximumAge ?? ""
      ].join(",");
    }

    this.#pendingCommand = undefined;
    this.#writable = false;
    this.#message.write(new Map([["LOCATION", value]]));
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
