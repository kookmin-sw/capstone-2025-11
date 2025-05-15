package com.capstone.whereigo;

public class PoseStamp {
    private final float x, z;
    private final int imageResId;

    public PoseStamp(float x, float z, int imageResId) {
        this.x = x;
        this.z = z;
        this.imageResId = imageResId;
    }

    public float x() {
        return this.x;
    }
    public float z() {
        return this.z;
    }
    public int imageResId() {
        return this.imageResId;
    }
}
