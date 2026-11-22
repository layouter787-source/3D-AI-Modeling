package com.layouter.aimodeling

/** Mesmos valores de `core::Primitive` no C++. */
enum class PrimitiveType(val id: Int, val apiName: String, val label: String) {
    CUBE(0, "cube", "Cubo"),
    SPHERE(1, "sphere", "Esfera"),
    CYLINDER(2, "cylinder", "Cilindro"),
    CONE(3, "cone", "Cone"),
    TORUS(4, "torus", "Torus"),
    PLANE(5, "plane", "Plano");

    companion object {
        fun fromApiName(name: String): PrimitiveType? = values().firstOrNull { it.apiName == name }
    }
}
