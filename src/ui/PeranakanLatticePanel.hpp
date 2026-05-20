#pragma once
#include <rack.hpp>

using namespace rack;

/**
 * PeranakanLatticePanel
 * 
 * High-density micro peranakan tile-inspired tech lattice texture.
 * Provides unified visual language across entire Red Dot Modular collection.
 * 
 * Features:
 * - Diagonal diamond lattice (forward + backward leaning lines)
 * - Tactile micro-dots at lattice intersection points
 * - Dynamic scaling based on module width
 * - Theme-aware colors (dark cyberpunk crimson / light minimalist slate)
 * - Ultra-thin stroke weight for high-density detail
 * 
 * Usage:
 *   struct MonsoonWidget : PeranakanLatticePanel {
 *       MonsoonWidget(Monsoon* module) : PeranakanLatticePanel(module) { }
 *   };
 */

struct PeranakanLatticePanel : app::SvgPanel {
    int* themeRef;  // 0 = Dark Theme, 1 = Light Theme
    
    PeranakanLatticePanel(int* theme = nullptr) : themeRef(theme) {}
    
    /**
     * Draws the peranakan lattice overlay on top of the base SVG panel.
     * Called by Rack's rendering system after base panel is drawn.
     */
    void draw(const DrawArgs& args) override {
        // 1. Draw the base SVG panel first (chassis, labels, component markings)
        app::SvgPanel::draw(args);
        
        // 2. Apply peranakan lattice overlay
        drawPeranakanLattice(args);
    }
    
protected:
    /**
     * Core lattice drawing logic
     * Separated into protected method for easy override in subclasses
     */
    virtual void drawPeranakanLattice(const DrawArgs& args) {
        nvgSave(args.vg);
        
        float width = box.size.x;
        float height = box.size.y;
        
        // --- DYNAMIC SPACING CALCULATION ---
        // Ultra-fine scaling: width / 30 gives excellent density
        // Example: 200px wide module → 6.67px spacing
        // Example: 120px wide module (Interchange) → 4.0px spacing
        float spacing = width / 30.0f;
        
        // --- THEME-DEPENDENT COLORS ---
        NVGcolor latticeColor;
        NVGcolor dotColor;
        
        if (themeRef && *themeRef == 0) {
            // DARK MODE: Cyberpunk Crimson Accents
            latticeColor = nvgRGBA(0xdc, 0x26, 0x26, 0x24); // #dc2626 @ 14% opacity
            dotColor     = nvgRGBA(0xdc, 0x26, 0x26, 0x3a); // #dc2626 @ 22% opacity
        } else {
            // LIGHT MODE: Minimalist Laboratory Slate Watermark
            latticeColor = nvgRGBA(0xe4, 0xe4, 0xe7, 0x70); // #e4e4e7 @ 44% opacity
            dotColor     = nvgRGBA(0xe4, 0xe4, 0xe7, 0x99); // #e4e4e7 @ 60% opacity
        }
        
        // --- STROKE WEIGHT OPTIMIZATION ---
        // Ultra-thin for high-density display legibility
        // Scales with panel width for consistent visual weight across sizes
        nvgStrokeWidth(args.vg, width / 800.0f);
        nvgStrokeColor(args.vg, latticeColor);
        
        // --- LAYER A: DIAGONAL DIAMOND LATTICE ---
        // Forward-leaning (/) and backward-leaning (\) diagonal lines
        // Creates interleaved diamond pattern across entire panel
        for (float x = -height; x < width + height; x += spacing * 2.0f) {
            // Forward-leaning diagonals (/)
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, x, 0);
            nvgLineTo(args.vg, x + height, height);
            nvgStroke(args.vg);
            
            // Backward-leaning diagonals (\)
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, x, height);
            nvgLineTo(args.vg, x + height, 0);
            nvgStroke(args.vg);
        }
        
        // --- LAYER B: CORE CENTER TACTILE NODES ---
        // Micro-dots placed at precise lattice intersection centers
        // Creates visual anchor points and enhances tactile quality
        nvgFillColor(args.vg, dotColor);
        float radius = width / 500.0f; // Scale dot size with panel width
        
        for (float x = 0; x <= width + spacing; x += spacing) {
            for (float y = 0; y <= height; y += spacing) {
                // Interlaced offset: every alternate row shifts by spacing/2
                // This places dots at diamond center intersection points
                int rowCheck = (int)(y / spacing);
                float offsetX = (rowCheck % 2 == 0) ? 0.0f : spacing / 2.0f;
                
                // Boundary check: keep dots within panel bounds
                if ((x + offsetX) >= 0 && (x + offsetX) <= width) {
                    nvgBeginPath(args.vg);
                    nvgCircle(args.vg, x + offsetX, y, radius);
                    nvgFill(args.vg);
                }
            }
        }
        
        nvgRestore(args.vg);
    }
};

/**
 * Specialized Lattice Panel Variants
 * These provide pre-tuned spacing/colors for different module categories
 */

/**
 * Monsoon/Large Panel Variant
 * For wide modules (~200px+) like main Monsoon sequencer
 */
struct PeranakanLatticePanelLarge : PeranakanLatticePanel {
    using PeranakanLatticePanel::PeranakanLatticePanel;
    
    void drawPeranakanLattice(const DrawArgs& args) override {
        nvgSave(args.vg);
        float width = box.size.x;
        float height = box.size.y;
        
        // Slightly coarser for large modules (still high-density)
        float spacing = width / 28.0f;
        
        NVGcolor latticeColor;
        NVGcolor dotColor;
        
        if (themeRef && *themeRef == 0) {
            latticeColor = nvgRGBA(0xdc, 0x26, 0x26, 0x28); // Slightly more opaque
            dotColor     = nvgRGBA(0xdc, 0x26, 0x26, 0x42);
        } else {
            latticeColor = nvgRGBA(0xe4, 0xe4, 0xe7, 0x78);
            dotColor     = nvgRGBA(0xe4, 0xe4, 0xe7, 0xa5);
        }
        
        nvgStrokeWidth(args.vg, width / 800.0f);
        nvgStrokeColor(args.vg, latticeColor);
        
        for (float x = -height; x < width + height; x += spacing * 2.0f) {
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, x, 0);
            nvgLineTo(args.vg, x + height, height);
            nvgStroke(args.vg);
            
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, x, height);
            nvgLineTo(args.vg, x + height, 0);
            nvgStroke(args.vg);
        }
        
        nvgFillColor(args.vg, dotColor);
        float radius = width / 500.0f;
        
        for (float x = 0; x <= width + spacing; x += spacing) {
            for (float y = 0; y <= height; y += spacing) {
                int rowCheck = (int)(y / spacing);
                float offsetX = (rowCheck % 2 == 0) ? 0.0f : spacing / 2.0f;
                
                if ((x + offsetX) >= 0 && (x + offsetX) <= width) {
                    nvgBeginPath(args.vg);
                    nvgCircle(args.vg, x + offsetX, y, radius);
                    nvgFill(args.vg);
                }
            }
        }
        
        nvgRestore(args.vg);
    }
};

/**
 * Compact Panel Variant
 * For narrow modules (~100px) like StraitsSands macro
 */
struct PeranakanLatticePanelCompact : PeranakanLatticePanel {
    using PeranakanLatticePanel::PeranakanLatticePanel;
    
    void drawPeranakanLattice(const DrawArgs& args) override {
        nvgSave(args.vg);
        float width = box.size.x;
        float height = box.size.y;
        
        // Finer spacing for compact modules
        float spacing = width / 32.0f;
        
        NVGcolor latticeColor;
        NVGcolor dotColor;
        
        if (themeRef && *themeRef == 0) {
            latticeColor = nvgRGBA(0xdc, 0x26, 0x26, 0x20); // Slightly less opaque for tiny space
            dotColor     = nvgRGBA(0xdc, 0x26, 0x26, 0x38);
        } else {
            latticeColor = nvgRGBA(0xe4, 0xe4, 0xe7, 0x68);
            dotColor     = nvgRGBA(0xe4, 0xe4, 0xe7, 0x8f);
        }
        
        nvgStrokeWidth(args.vg, width / 900.0f); // Thinner stroke for compact
        nvgStrokeColor(args.vg, latticeColor);
        
        for (float x = -height; x < width + height; x += spacing * 2.0f) {
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, x, 0);
            nvgLineTo(args.vg, x + height, height);
            nvgStroke(args.vg);
            
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, x, height);
            nvgLineTo(args.vg, x + height, 0);
            nvgStroke(args.vg);
        }
        
        nvgFillColor(args.vg, dotColor);
        float radius = width / 550.0f; // Slightly smaller dots
        
        for (float x = 0; x <= width + spacing; x += spacing) {
            for (float y = 0; y <= height; y += spacing) {
                int rowCheck = (int)(y / spacing);
                float offsetX = (rowCheck % 2 == 0) ? 0.0f : spacing / 2.0f;
                
                if ((x + offsetX) >= 0 && (x + offsetX) <= width) {
                    nvgBeginPath(args.vg);
                    nvgCircle(args.vg, x + offsetX, y, radius);
                    nvgFill(args.vg);
                }
            }
        }
        
        nvgRestore(args.vg);
    }
};

/**
 * Deep/Wide Panel Variant
 * For very wide modules (55mm+) like DeepStraitsSands
 */
struct PeranakanLatticePanelDeep : PeranakanLatticePanel {
    using PeranakanLatticePanel::PeranakanLatticePanel;
    
    void drawPeranakanLattice(const DrawArgs& args) override {
        nvgSave(args.vg);
        float width = box.size.x;
        float height = box.size.y;
        
        // Even coarser for deep modules to avoid visual chaos
        float spacing = width / 24.0f;
        
        NVGcolor latticeColor;
        NVGcolor dotColor;
        
        if (themeRef && *themeRef == 0) {
            latticeColor = nvgRGBA(0xdc, 0x26, 0x26, 0x2c);
            dotColor     = nvgRGBA(0xdc, 0x26, 0x26, 0x48);
        } else {
            latticeColor = nvgRGBA(0xe4, 0xe4, 0xe7, 0x80);
            dotColor     = nvgRGBA(0xe4, 0xe4, 0xe7, 0xb0);
        }
        
        nvgStrokeWidth(args.vg, width / 780.0f);
        nvgStrokeColor(args.vg, latticeColor);
        
        for (float x = -height; x < width + height; x += spacing * 2.0f) {
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, x, 0);
            nvgLineTo(args.vg, x + height, height);
            nvgStroke(args.vg);
            
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, x, height);
            nvgLineTo(args.vg, x + height, 0);
            nvgStroke(args.vg);
        }
        
        nvgFillColor(args.vg, dotColor);
        float radius = width / 480.0f; // Larger dots for deep modules
        
        for (float x = 0; x <= width + spacing; x += spacing) {
            for (float y = 0; y <= height; y += spacing) {
                int rowCheck = (int)(y / spacing);
                float offsetX = (rowCheck % 2 == 0) ? 0.0f : spacing / 2.0f;
                
                if ((x + offsetX) >= 0 && (x + offsetX) <= width) {
                    nvgBeginPath(args.vg);
                    nvgCircle(args.vg, x + offsetX, y, radius);
                    nvgFill(args.vg);
                }
            }
        }
        
        nvgRestore(args.vg);
    }
};
