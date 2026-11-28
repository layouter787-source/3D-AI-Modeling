package com.layouter.aimodeling

/** Mesmos valores de `core::Primitive` no C++. */
enum class PrimitiveType(val id: Int, val apiName: String, val label: String) {
    CUBE(0, "cube", "Cubo"),
    SPHERE(1, "sphere", "Esfera"),
    CYLINDER(2, "cylinder", "Cilindro"),
    CONE(3, "cone", "Cone"),
    TORUS(4, "torus", "Torus"),
    PLANE(5, "plane", "Plano"),
    PYRAMID(6, "pyramid", "Pirâmide"),
    CAPSULE(7, "capsule", "Cápsula"),
    OCTAHEDRON(8, "octahedron", "Octaedro"),
    HEX_PRISM(9, "hex_prism", "Prisma hexagonal"),
    CUSTOM_MESH(10, "custom_mesh", "Malha personalizada"),
    TRI_PRISM(11, "tri_prism", "Prisma triangular"),
    PENT_PRISM(12, "pent_prism", "Prisma pentagonal"),
    OCT_PRISM(13, "oct_prism", "Prisma octogonal"),
    DEC_PRISM(14, "dec_prism", "Prisma decagonal"),
    DODEC_PRISM(15, "dodec_prism", "Prisma dodecagonal"),
    TETRAHEDRON(16, "tetrahedron", "Tetraedro"),
    ICOSAHEDRON(17, "icosahedron", "Icosaedro"),
    ICOSPHERE(18, "icosphere", "Icosfera"),
    PIPE(19, "pipe", "Tubo");

    companion object {
        fun fromApiName(name: String): PrimitiveType? = values().firstOrNull { it.apiName == name }
    }
}
