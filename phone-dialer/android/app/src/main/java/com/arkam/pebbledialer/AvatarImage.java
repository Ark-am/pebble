package com.arkam.pebbledialer;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;

/**
 * Turns a contact photo into the round picture the watch shows on its call
 * screen, in Pebble's 8-bit colour format: one byte per pixel, 2 bits each of
 * alpha, red, green and blue (64 colours). Corners outside the circle are
 * transparent.
 */
final class AvatarImage {
    // Pebble's colour screens are reflective and look muted; extra saturation
    // keeps faces from turning grey after reducing to 64 colours.
    private static final float SATURATION = 1.5f;
    // Half-strength dithering keeps the shading but avoids the speckle that
    // full Floyd-Steinberg gives in pale areas with so few colours.
    private static final float DITHER = 0.5f;

    private AvatarImage() { }

    /** The photo as size × size Pebble pixels, row by row. */
    static byte[] toPebble(Bitmap photo, int size) {
        // Centre-crop to a square and scale down with filtering.
        int side = Math.min(photo.getWidth(), photo.getHeight());
        Rect crop = new Rect((photo.getWidth() - side) / 2, (photo.getHeight() - side) / 2,
                (photo.getWidth() + side) / 2, (photo.getHeight() + side) / 2);
        Bitmap scaled = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888);
        new Canvas(scaled).drawBitmap(photo, crop, new Rect(0, 0, size, size),
                new Paint(Paint.FILTER_BITMAP_FLAG));
        int[] argb = new int[size * size];
        scaled.getPixels(argb, 0, size, 0, 0, size, size);
        scaled.recycle();

        float[][] rgb = new float[3][size * size];
        for (int i = 0; i < argb.length; i++) {
            float r = (argb[i] >> 16) & 0xff;
            float g = (argb[i] >> 8) & 0xff;
            float b = argb[i] & 0xff;
            float grey = 0.299f * r + 0.587f * g + 0.114f * b;
            rgb[0][i] = grey + (r - grey) * SATURATION;
            rgb[1][i] = grey + (g - grey) * SATURATION;
            rgb[2][i] = grey + (b - grey) * SATURATION;
        }

        // Floyd-Steinberg dithering to 4 levels per channel (0, 85, 170, 255),
        // at half strength, which keeps faces recognisable with 64 colours.
        byte[] pebble = new byte[size * size];
        float centre = (size - 1) / 2f;
        float radius = size / 2f;
        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                int i = y * size + x;
                int pixel = 0xc0;  // Opaque.
                for (int channel = 0; channel < 3; channel++) {
                    float value = Math.max(0, Math.min(255, rgb[channel][i]));
                    int level = Math.round(value / 85f);
                    float error = (value - level * 85f) * DITHER;
                    spread(rgb[channel], size, x, y, error);
                    pixel |= level << (4 - 2 * channel);
                }
                float dx = x - centre;
                float dy = y - centre;
                // Transparent outside the circle.
                pebble[i] = (byte) (dx * dx + dy * dy <= radius * radius ? pixel : 0);
            }
        }
        return pebble;
    }

    private static void spread(float[] channel, int size, int x, int y, float error) {
        add(channel, size, x + 1, y, error * 7 / 16);
        add(channel, size, x - 1, y + 1, error * 3 / 16);
        add(channel, size, x, y + 1, error * 5 / 16);
        add(channel, size, x + 1, y + 1, error / 16);
    }

    private static void add(float[] channel, int size, int x, int y, float amount) {
        if (x >= 0 && x < size && y < size) {
            channel[y * size + x] += amount;
        }
    }
}
