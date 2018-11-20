struct vdbMat4
{
    float data[4*4];
    #if defined(VDB_MATRIX_COLUMN_MAJOR)
    float &at(int row, int col) { return data[col + row*4]; }
    #else
    float &at(int row, int col) { return data[row + col*4]; }
    #endif
};

static vdbVec4 operator-(vdbVec4 a)              { return vdbVec4(-a.x, -a.y, -a.z, -a.w); }
static vdbVec4 operator+(vdbVec4 a, vdbVec4 b)   { return vdbVec4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w); }
static vdbVec4 operator-(vdbVec4 a, vdbVec4 b)   { return vdbVec4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w); }
static vdbVec4 operator*(vdbVec4 a, vdbVec4 b)   { return vdbVec4(a.x*b.x, a.y*b.y, a.z*b.z, a.w*b.w); }
static vdbVec4 operator*(vdbVec4 a, float b)     { return vdbVec4(a.x*b, a.y*b, a.z*b, a.w*b); }
static vdbVec4 operator*(float b, vdbVec4 a)     { return vdbVec4(a.x*b, a.y*b, a.z*b, a.w*b); }
static vdbVec4 operator/(vdbVec4 a, float b)     { return vdbVec4(a.x/b, a.y/b, a.z/b, a.w/b); }
static vdbVec4 operator/(float b, vdbVec4 a)     { return vdbVec4(a.x/b, a.y/b, a.z/b, a.w/b); }
static float vdbVecLength(vdbVec4 v)             { return sqrtf(v.x*v.x + v.y*v.y + v.z*v.z + v.w*v.w); }
static float vdbVecDot(vdbVec4 a, vdbVec4 b)     { return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w; }
static vdbVec4 vdbVecNormalize(vdbVec4 v)
{
    float length = vdbVecLength(v);
    if (length == 0.0f)
        return vdbVec4(0.0f,0.0f,0.0f,0.0f);
    else
        return v*(1.0f/length);
}

static vdbVec3 operator-(vdbVec3 a)              { return vdbVec3(-a.x, -a.y, -a.z); }
static vdbVec3 operator+(vdbVec3 a, vdbVec3 b)   { return vdbVec3(a.x + b.x, a.y + b.y, a.z + b.z); }
static vdbVec3 operator-(vdbVec3 a, vdbVec3 b)   { return vdbVec3(a.x - b.x, a.y - b.y, a.z - b.z); }
static vdbVec3 operator*(vdbVec3 a, vdbVec3 b)   { return vdbVec3(a.x*b.x, a.y*b.y, a.z*b.z); }
static vdbVec3 operator*(vdbVec3 a, float b)     { return vdbVec3(a.x*b, a.y*b, a.z*b); }
static vdbVec3 operator*(float b, vdbVec3 a)     { return vdbVec3(a.x*b, a.y*b, a.z*b); }
static vdbVec3 operator/(vdbVec3 a, float b)     { return vdbVec3(a.x/b, a.y/b, a.z/b); }
static vdbVec3 operator/(float b, vdbVec3 a)     { return vdbVec3(a.x/b, a.y/b, a.z/b); }
static float vdbVecLength(vdbVec3 v)             { return sqrtf(v.x*v.x + v.y*v.y + v.z*v.z); }
static float vdbVecDot(vdbVec3 a, vdbVec3 b)     { return a.x*b.x + a.y*b.y + a.z*b.z; }
static vdbVec3 vdbVecNormalize(vdbVec3 v)
{
    float length = vdbVecLength(v);
    if (length == 0.0f)
        return vdbVec3(0.0f,0.0f,0.0f);
    else
        return v*(1.0f/length);
}
static vdbVec3 vdbVecCross(vdbVec3 a, vdbVec3 b)
{
    return vdbVec3(a.y*b.z - a.z*b.y,
                   a.z*b.x - a.x*b.z,
                   a.x*b.y - a.y*b.x);
}

static vdbMat4 vdbInitMat4(float a00, float a01, float a02, float a03,
                           float a10, float a11, float a12, float a13,
                           float a20, float a21, float a22, float a23,
                           float a30, float a31, float a32, float a33)
{
    #ifdef VDB_MATRIX_COLUMN_MAJOR
    vdbMat4 a = {
        a00, a01, a02, a03,
        a10, a11, a12, a13,
        a20, a21, a22, a23,
        a30, a31, a32, a33
    };
    #else
    vdbMat4 a = {
        a00, a10, a20, a30,
        a01, a11, a21, a31,
        a02, a12, a22, a32,
        a03, a13, a23, a33
    };
    #endif
    return a;
}

static vdbMat4 operator+(vdbMat4 a, vdbMat4 b)
{
    vdbMat4 c = {0};
    for (int i = 0; i < 4*4; i++)
        c.data[i] = a.data[i] + b.data[i];
    return c;
}

static vdbMat4 operator*(vdbMat4 a, float b)
{
    vdbMat4 c = {0};
    for (int i = 0; i < 4*4; i++)
        c.data[i] = a.data[i]*b;
    return c;
}

static vdbMat4 operator/(vdbMat4 a, float b)
{
    vdbMat4 c = {0};
    for (int i = 0; i < 4*4; i++)
        c.data[i] = a.data[i]/b;
    return c;
}

static vdbMat4 operator*(float b, vdbMat4 a) { return a*b; }
static vdbMat4 operator/(float b, vdbMat4 a) { return a/b; }
static vdbMat4 vdbMul4x4(vdbMat4 a, vdbMat4 b)
{
    vdbMat4 c = {0};
    for (int row = 0; row < 4; row++)
    for (int col = 0; col < 4; col++)
    {
        c.at(row,col) = 0.0f;
        for (int i = 0; i < 4; i++)
            c.at(row,col) += a.at(row,i)*b.at(i,col);
    }
    return c;
}
static vdbMat4 operator*(vdbMat4 a, vdbMat4 b) { return vdbMul4x4(a,b); }

static vdbVec4 vdbMul4x1(vdbMat4 a, vdbVec4 b)
{
    vdbVec4 c(0.0f,0.0f,0.0f,0.0f);
    c.x = b.x*a.at(0,0) + b.y*a.at(0,1) + b.z*a.at(0,2) + b.w*a.at(0,3);
    c.y = b.x*a.at(1,0) + b.y*a.at(1,1) + b.z*a.at(1,2) + b.w*a.at(1,3);
    c.z = b.x*a.at(2,0) + b.y*a.at(2,1) + b.z*a.at(2,2) + b.w*a.at(2,3);
    c.w = b.x*a.at(3,0) + b.y*a.at(3,1) + b.z*a.at(3,2) + b.w*a.at(3,3);
    return c;
}
static vdbVec4 operator*(vdbMat4 a, vdbVec4 b) { return vdbMul4x1(a,b); }

static vdbVec4 vdbMulTranspose4x1(vdbMat4 a, vdbVec4 b)
{
    vdbVec4 c(0.0f,0.0f,0.0f,0.0f);
    c.x = b.x*a.at(0,0) + b.y*a.at(1,0) + b.z*a.at(2,0) + b.w*a.at(3,0);
    c.y = b.x*a.at(0,1) + b.y*a.at(1,1) + b.z*a.at(2,1) + b.w*a.at(3,1);
    c.z = b.x*a.at(0,2) + b.y*a.at(1,2) + b.z*a.at(2,2) + b.w*a.at(3,2);
    c.w = b.x*a.at(0,3) + b.y*a.at(1,3) + b.z*a.at(2,3) + b.w*a.at(3,3);
    return c;
}

static vdbVec4 vdbMulSE3Inverse(vdbMat4 a, vdbVec4 b)
{
    vdbVec4 c(b.x-a.at(0,3)*b.w, b.y-a.at(1,3)*b.w, b.z-a.at(2,3)*b.w, b.w);
    vdbVec4 d(0.0f,0.0f,0.0f,0.0f);
    d.x = c.x*a.at(0,0) + c.y*a.at(1,0) + c.z*a.at(2,0);
    d.y = c.x*a.at(0,1) + c.y*a.at(1,1) + c.z*a.at(2,1);
    d.z = c.x*a.at(0,2) + c.y*a.at(1,2) + c.z*a.at(2,2);
    d.w = c.w;
    return d;
}

static vdbMat4 vdbMatIdentity()
{
    vdbMat4 result = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    return result;
}

static vdbMat4 vdbMatRotateZ(float t)
{
    vdbMat4 a = vdbMatIdentity();
    a.at(0,0) = cosf(t); a.at(0,1) = -sinf(t);
    a.at(1,0) = sinf(t); a.at(1,1) = cosf(t);
    return a;
}

static vdbMat4 vdbMatRotateY(float t)
{
    vdbMat4 a = vdbMatIdentity();
    a.at(0,0) =  cosf(t); a.at(0,2) = sinf(t);
    a.at(2,0) = -sinf(t); a.at(2,2) = cosf(t);
    return a;
}

static vdbMat4 vdbMatRotateX(float t)
{
    vdbMat4 a = vdbMatIdentity();
    a.at(1,1) = cosf(t); a.at(1,2) = -sinf(t);
    a.at(2,1) = sinf(t); a.at(2,2) = cosf(t);
    return a;
}

static vdbMat4 vdbMatTranslate(float x, float y, float z)
{
    vdbMat4 a = vdbMatIdentity();
    a.at(0,3) = x;
    a.at(1,3) = y;
    a.at(2,3) = z;
    return a;
}

// M = Rx*Ry*Rz
static vdbMat4 vdbMatRotateXYZ(float rx,float ry,float rz)
{
    vdbMat4 Rx = vdbMatRotateX(rx);
    vdbMat4 Ry = vdbMatRotateY(ry);
    vdbMat4 Rz = vdbMatRotateZ(rz);
    vdbMat4 M = vdbMul4x4(Ry,Rz);
            M = vdbMul4x4(Rx, M);
    return M;
}

// M = Rz*Ry*Rx
static vdbMat4 vdbMatRotateZYX(float rz,float ry,float rx)
{
    vdbMat4 Rx = vdbMatRotateX(rx);
    vdbMat4 Ry = vdbMatRotateY(ry);
    vdbMat4 Rz = vdbMatRotateZ(rz);
    vdbMat4 M = vdbMul4x4(Ry,Rx);
            M = vdbMul4x4(Rz, M);
    return M;
}

static vdbMat4 vdbMatSkew(vdbVec3 v)
{
    return vdbInitMat4(
      0 , -v.z,  v.y,  0,
     v.z,   0 , -v.x,  0,
    -v.y,  v.x,   0 ,  0,
      0 ,   0 ,   0 ,  0
    );
}

static vdbMat4 vdbMatOrthogonalize(vdbMat4 R)
// Orthogonalizes the upper-left 3x3 matrix of R.
{
    vdbVec3 x = vdbVec3(R.at(0,0), R.at(1,0), R.at(2,0));
    vdbVec3 y = vdbVec3(R.at(0,1), R.at(1,1), R.at(2,1));
    float e = vdbVecDot(x, y);
    vdbVec3 ny = vdbVecNormalize((y - 0.5f*e*x)/(1.0f-0.25f*e*e));
    vdbVec3 nx = vdbVecNormalize((x - 0.5f*e*ny));
    vdbVec3 nz = vdbVecCross(nx, ny);
    vdbMat4 result = vdbInitMat4(
        nx.x,       ny.x,      nz.x,      R.at(0,3),
        nx.y,       ny.y,      nz.y,      R.at(1,3),
        nx.z,       ny.z,      nz.z,      R.at(2,3),
        R.at(3,0),  R.at(3,1), R.at(3,2), R.at(3,3)
    );
    return result;
}

#if 0
static vdbVec4 vdbMulInverse()
{
    // http://stackoverflow.com/questions/1148309/inverting-a-4x4-matrix
    vdb_mat4 inv;
    inv[0] = m[5]  * m[10] * m[15] - m[5]  * m[11] * m[14] - m[9]  * m[6]  * m[15] + m[9]  * m[7]  * m[14] + m[13] * m[6]  * m[11] - m[13] * m[7]  * m[10];
    inv[4] = -m[4]  * m[10] * m[15] + m[4]  * m[11] * m[14] + m[8]  * m[6]  * m[15] - m[8]  * m[7]  * m[14] - m[12] * m[6]  * m[11] + m[12] * m[7]  * m[10];
    inv[8] = m[4]  * m[9] * m[15] - m[4]  * m[11] * m[13] - m[8]  * m[5] * m[15] + m[8]  * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4]  * m[9] * m[14] + m[4]  * m[10] * m[13] + m[8]  * m[5] * m[14] - m[8]  * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1]  * m[10] * m[15] + m[1]  * m[11] * m[14] + m[9]  * m[2] * m[15] - m[9]  * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0]  * m[10] * m[15] - m[0]  * m[11] * m[14] - m[8]  * m[2] * m[15] + m[8]  * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0]  * m[9] * m[15] + m[0]  * m[11] * m[13] + m[8]  * m[1] * m[15] - m[8]  * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0]  * m[9] * m[14] - m[0]  * m[10] * m[13] - m[8]  * m[1] * m[14] + m[8]  * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1]  * m[6] * m[15] - m[1]  * m[7] * m[14] - m[5]  * m[2] * m[15] + m[5]  * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0]  * m[6] * m[15] + m[0]  * m[7] * m[14] + m[4]  * m[2] * m[15] - m[4]  * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0]  * m[5] * m[15] - m[0]  * m[7] * m[13] - m[4]  * m[1] * m[15] + m[4]  * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0]  * m[5] * m[14] + m[0]  * m[6] * m[13] + m[4]  * m[1] * m[14] - m[4]  * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

    float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

    if (det == 0)
        return false;

    for (int i = 0; i < 16; i++)
        inv[i] /= det;

    *y = vdb_mul4x1(inv, x);
}
#endif
