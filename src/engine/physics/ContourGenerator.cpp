#include "engine/physics/ContourGenerator.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace engine::physics {

// --- Marching squares ---

// Each 2x2 cell has 4 corners: TL(bit0), TR(bit1), BR(bit2), BL(bit3).
// The 16 cases define which edge segments to emit.
// Segment direction: solid is on the left side of the segment direction.
std::vector<ContourGenerator::Segment> ContourGenerator::marching_squares(
    const bool* grid, int w, int h)
{
    std::vector<Segment> segments;
    segments.reserve(w * h / 4); // rough estimate

    // Iterate over (w+1) x (h+1) cells (extend grid by 1 in each direction
    // so we get boundary segments at the edges of the grid).
    // To sample a corner at (cx, cy), we check grid[cy * w + cx] if in bounds,
    // otherwise treat as empty.
    auto sample = [&](int x, int y) -> bool {
        if (x < 0 || x >= w || y < 0 || y >= h) return false;
        return grid[y * w + x];
    };

    for (int cy = -1; cy < h; cy++) {
        for (int cx = -1; cx < w; cx++) {
            // The 2x2 block corners in grid coordinates:
            //   TL = (cx, cy)      TR = (cx+1, cy)
            //   BL = (cx, cy+1)    BR = (cx+1, cy+1)
            int idx = 0;
            if (sample(cx,     cy))     idx |= 1;  // TL
            if (sample(cx + 1, cy))     idx |= 2;  // TR
            if (sample(cx + 1, cy + 1)) idx |= 4;  // BR
            if (sample(cx,     cy + 1)) idx |= 8;  // BL

            if (idx == 0 || idx == 15) continue;

            // Cell position in pixel coordinates (top-left corner of the 2x2 block)
            float fx = static_cast<float>(cx);
            float fy = static_cast<float>(cy);

            // Edge midpoints
            Vec2f top    = {fx + 1.0f, fy + 0.5f};
            Vec2f right  = {fx + 1.5f, fy + 1.0f};
            Vec2f bottom = {fx + 1.0f, fy + 1.5f};
            Vec2f left   = {fx + 0.5f, fy + 1.0f};

            // Segment direction: solid on the left side of the segment direction.
            switch (idx) {
            // Single corner
            case 1:  segments.push_back({left, top}); break;
            case 2:  segments.push_back({top, right}); break;
            case 4:  segments.push_back({right, bottom}); break;
            case 8:  segments.push_back({bottom, left}); break;

            // Two adjacent corners
            case 3:  segments.push_back({left, right}); break;
            case 6:  segments.push_back({top, bottom}); break;
            case 12: segments.push_back({right, left}); break;
            case 9:  segments.push_back({bottom, top}); break;

            // Diagonal (saddle) — connect to avoid crossing
            case 5:
                segments.push_back({left, top});
                segments.push_back({right, bottom});
                break;
            case 10:
                segments.push_back({top, right});
                segments.push_back({bottom, left});
                break;

            // Three corners (inverse single)
            case 7:  segments.push_back({left, bottom}); break;
            case 11: segments.push_back({bottom, right}); break;
            case 13: segments.push_back({right, top}); break;
            case 14: segments.push_back({top, left}); break;
            }
        }
    }

    return segments;
}

// --- Segment joining ---

// Hash a Vec2f for use in an unordered_map. We quantize to avoid floating-point issues.
struct Vec2fHash {
    std::size_t operator()(const Vec2f& v) const {
        // Quantize to 0.25 pixel precision (our midpoints are at 0.5 intervals)
        int ix = static_cast<int>(v.x * 4.0f + 0.5f);
        int iy = static_cast<int>(v.y * 4.0f + 0.5f);
        return std::hash<int>()(ix) ^ (std::hash<int>()(iy) << 16);
    }
};

struct Vec2fEqual {
    bool operator()(const Vec2f& a, const Vec2f& b) const {
        return std::abs(a.x - b.x) < 0.01f && std::abs(a.y - b.y) < 0.01f;
    }
};

std::vector<std::vector<Vec2f>> ContourGenerator::join_segments(
    const std::vector<Segment>& segs)
{
    if (segs.empty()) return {};

    // Build adjacency: for each endpoint, store which segment indices connect there.
    // Each segment contributes its 'b' endpoint as the "next" from 'a'.
    std::unordered_map<Vec2f, Vec2f, Vec2fHash, Vec2fEqual> next_map;
    for (auto& seg : segs) {
        next_map[seg.a] = seg.b;
    }

    // Track which start points have been visited
    std::unordered_map<Vec2f, bool, Vec2fHash, Vec2fEqual> visited;

    std::vector<std::vector<Vec2f>> polygons;

    for (auto& seg : segs) {
        if (visited[seg.a]) continue;

        std::vector<Vec2f> chain;
        Vec2f current = seg.a;
        int safety = 0;
        int max_steps = static_cast<int>(segs.size()) + 1;

        while (safety++ < max_steps) {
            if (visited[current]) break;
            visited[current] = true;
            chain.push_back(current);

            auto it = next_map.find(current);
            if (it == next_map.end()) break;
            current = it->second;
        }

        if (chain.size() >= 3) {
            polygons.push_back(std::move(chain));
        }
    }

    return polygons;
}

// --- RDP simplification ---

static float perpendicular_distance(const Vec2f& p, const Vec2f& a, const Vec2f& b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float len_sq = dx * dx + dy * dy;
    if (len_sq < 1e-10f) {
        // a and b are the same point
        float ex = p.x - a.x;
        float ey = p.y - a.y;
        return std::sqrt(ex * ex + ey * ey);
    }
    float area2 = std::abs(dx * (a.y - p.y) - (a.x - p.x) * dy);
    return area2 / std::sqrt(len_sq);
}

static void rdp_recursive(const std::vector<Vec2f>& points, int start, int end,
                           float epsilon, std::vector<bool>& keep) {
    if (end - start < 2) return;

    float max_dist = 0.0f;
    int max_idx = start;

    for (int i = start + 1; i < end; i++) {
        float d = perpendicular_distance(points[i], points[start], points[end]);
        if (d > max_dist) {
            max_dist = d;
            max_idx = i;
        }
    }

    if (max_dist > epsilon) {
        keep[max_idx] = true;
        rdp_recursive(points, start, max_idx, epsilon, keep);
        rdp_recursive(points, max_idx, end, epsilon, keep);
    }
}

std::vector<Vec2f> ContourGenerator::simplify_rdp(const std::vector<Vec2f>& poly, float epsilon) {
    if (poly.size() < 4) return poly;

    int n = static_cast<int>(poly.size());

    // For closed polygons, we find the point farthest from the line between
    // index 0 and n/2, then simplify each half independently.
    int mid = n / 2;
    std::vector<bool> keep(n, false);
    keep[0] = true;
    keep[mid] = true;

    // Find the actual farthest point to use as a better split
    float best_dist = 0.0f;
    for (int i = 1; i < n; i++) {
        float d = perpendicular_distance(poly[i], poly[0], poly[mid]);
        if (d > best_dist) {
            best_dist = d;
            mid = i;
            keep.assign(n, false);
            keep[0] = true;
            keep[mid] = true;
        }
    }

    rdp_recursive(poly, 0, mid, epsilon, keep);
    rdp_recursive(poly, mid, n - 1, epsilon, keep);

    // Also handle wrap-around: from last point back to first
    keep[n - 1] = true;
    // We already simplified 0..mid and mid..n-1

    std::vector<Vec2f> result;
    for (int i = 0; i < n; i++) {
        if (keep[i]) result.push_back(poly[i]);
    }

    return result;
}

// --- Signed area ---

float ContourGenerator::signed_area(const std::vector<Vec2f>& poly) {
    float area = 0.0f;
    int n = static_cast<int>(poly.size());
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        area += poly[i].x * poly[j].y;
        area -= poly[j].x * poly[i].y;
    }
    return area * 0.5f;
}

// --- Full pipeline ---

std::vector<Contour> ContourGenerator::generate(
    const bool* solid_grid, int width, int height, float epsilon)
{
    // Step 1: Marching squares
    auto segments = marching_squares(solid_grid, width, height);
    if (segments.empty()) return {};

    // Step 2: Join segments into closed polygons
    auto polygons = join_segments(segments);

    // Step 3: Simplify each polygon and classify
    std::vector<Contour> contours;
    contours.reserve(polygons.size());

    for (auto& poly : polygons) {
        Contour c;
        c.vertices = simplify_rdp(poly, epsilon);

        // Need at least 3 vertices for a valid contour
        if (c.vertices.size() < 3) continue;

        // Snap vertices to half-pixel grid (0.5 increments) for cleaner edges
        // and to ensure chunk boundary alignment
        for (auto& v : c.vertices) {
            v.x = std::round(v.x * 2.0f) * 0.5f;
            v.y = std::round(v.y * 2.0f) * 0.5f;
        }

        // Remove collinear vertices after snapping (eliminates tiny bumps at chunk boundaries)
        std::vector<Vec2f> cleaned;
        int n = static_cast<int>(c.vertices.size());
        for (int i = 0; i < n; i++) {
            const Vec2f& prev = c.vertices[(i - 1 + n) % n];
            const Vec2f& curr = c.vertices[i];
            const Vec2f& next = c.vertices[(i + 1) % n];

            // Check if curr is collinear with prev and next
            float dx1 = curr.x - prev.x;
            float dy1 = curr.y - prev.y;
            float dx2 = next.x - curr.x;
            float dy2 = next.y - curr.y;

            // Cross product - if zero or very small, points are collinear
            float cross = dx1 * dy2 - dy1 * dx2;
            if (std::abs(cross) > 0.01f) {
                cleaned.push_back(curr);
            }
        }

        if (cleaned.size() < 3) continue;
        c.vertices = std::move(cleaned);

        // In screen coordinates (Y-down), marching squares produces CCW
        // outer boundaries (negative signed area via shoelace) and CW
        // hole boundaries (positive signed area).
        float area = signed_area(c.vertices);
        c.is_hole = (area > 0.0f);

        contours.push_back(std::move(c));
    }

    return contours;
}

} // namespace engine::physics
