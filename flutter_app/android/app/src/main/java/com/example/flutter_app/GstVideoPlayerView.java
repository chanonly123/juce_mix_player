package com.example.flutter_app;

import android.content.Context;
import android.graphics.SurfaceTexture;
import android.view.Surface;
import android.view.TextureView;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.util.Map;

import io.flutter.plugin.common.StandardMessageCodec;
import io.flutter.plugin.platform.PlatformView;
import io.flutter.plugin.platform.PlatformViewFactory;

public class GstVideoPlayerView implements PlatformView, TextureView.SurfaceTextureListener {
    static {
        System.loadLibrary("juce_jni");
    }

    private final TextureView textureView;
    private final long playerPtr;
    private Surface currentSurface;

    public GstVideoPlayerView(@NonNull Context context, int id, @Nullable Map<String, Object> creationParams) {
        textureView = new TextureView(context);
        textureView.setSurfaceTextureListener(this);

        if (creationParams != null && creationParams.containsKey("playerPtr")) {
            Object ptrObj = creationParams.get("playerPtr");
            if (ptrObj instanceof Long) {
                playerPtr = (Long) ptrObj;
            } else if (ptrObj instanceof Integer) {
                playerPtr = ((Integer) ptrObj).longValue();
            } else {
                playerPtr = 0;
            }
        } else {
            playerPtr = 0;
        }
    }

    @Override
    public View getView() {
        return textureView;
    }

    @Override
    public void dispose() {
        if (playerPtr != 0) {
            nativeSetSurfaceHandle(playerPtr, null);
        }
        if (currentSurface != null) {
            currentSurface.release();
            currentSurface = null;
        }
    }

    @Override
    public void onSurfaceTextureAvailable(@NonNull SurfaceTexture surfaceTexture, int width, int height) {
        currentSurface = new Surface(surfaceTexture);
        if (playerPtr != 0) {
            nativeSetSurfaceHandle(playerPtr, currentSurface);
        }
    }

    @Override
    public void onSurfaceTextureSizeChanged(@NonNull SurfaceTexture surfaceTexture, int width, int height) {
    }

    @Override
    public boolean onSurfaceTextureDestroyed(@NonNull SurfaceTexture surfaceTexture) {
        if (playerPtr != 0) {
            nativeSetSurfaceHandle(playerPtr, null);
        }
        if (currentSurface != null) {
            currentSurface.release();
            currentSurface = null;
        }
        return true;
    }

    @Override
    public void onSurfaceTextureUpdated(@NonNull SurfaceTexture surfaceTexture) {
    }

    private native void nativeSetSurfaceHandle(long playerPtr, Object surface);

    public static class Factory extends PlatformViewFactory {
        public Factory() {
            super(StandardMessageCodec.INSTANCE);
        }

        @NonNull
        @Override
        @SuppressWarnings("unchecked")
        public PlatformView create(@NonNull Context context, int viewId, @Nullable Object args) {
            Map<String, Object> creationParams = (Map<String, Object>) args;
            return new GstVideoPlayerView(context, viewId, creationParams);
        }
    }
}
