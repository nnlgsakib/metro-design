# metro-blobtrack

Detect and track connected components (blobs) across frames with centroid tracking, bounding box overlay, and motion trails.

## Parameters

| Parameter     | Type  | Default | Description                        |
|---------------|-------|---------|------------------------------------|
| Enable        | bool  | true    | Enable blob detection              |
| Threshold     | float | 0.5     | Binarization threshold             |
| MinBlobSize   | int   | 10      | Minimum blob area (pixels)         |
| MaxBlobSize   | int   | 10000   | Maximum blob area (pixels)         |
| ProxMerge     | int   | 5       | Proximity radius for blob merging  |
| ShowOverlay   | bool  | true    | Show bounding box overlay          |
| ShowTrails    | bool  | true    | Show motion trails                 |
| TrailLength   | int   | 30      | Motion trail length (frames)       |
| OverlayOpacity| float | 0.8     | Overlay opacity                    |
| Opacity       | float | 1.0     | Output opacity                     |
