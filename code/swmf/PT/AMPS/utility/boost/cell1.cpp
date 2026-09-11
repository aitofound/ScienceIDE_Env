#include <Python.h>
#include <vector>
#include <map>
#include <string>
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <iostream>

// Basic 3D vector class
class Vector3D {
public:
    double x, y, z;
    Vector3D(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z) {}
};

// Matrix class for vertex data
class Matrix {
public:
    Matrix(size_t rows = 0, size_t cols = 0) : rows_(rows), cols_(cols) {
        if (rows_ > 0 && cols_ > 0) {
            data_.resize(rows_ * cols_);
        }
    }

    double& operator()(size_t row, size_t col) {
        return data_[row * cols_ + col];
    }

    const double& operator()(size_t row, size_t col) const {
        return data_[row * cols_ + col];
    }

    size_t rows() const { return rows_; }
    size_t cols() const { return cols_; }

private:
    std::vector<double> data_;
    size_t rows_, cols_;
};

// Integer matrix for tetrahedra
class IntMatrix {
public:
    IntMatrix(size_t rows = 0, size_t cols = 0) : rows_(rows), cols_(cols) {
        if (rows_ > 0 && cols_ > 0) {
            data_.resize(rows_ * cols_);
        }
    }

    int& operator()(size_t row, size_t col) {
        return data_[row * cols_ + col];
    }

    const int& operator()(size_t row, size_t col) const {
        return data_[row * cols_ + col];
    }

    size_t rows() const { return rows_; }
    size_t cols() const { return cols_; }

private:
    std::vector<int> data_;
    size_t rows_, cols_;
};

class PythonWrapper {
public:
    PythonWrapper() {
        Py_Initialize();
        mainModule_ = PyImport_ImportModule("__main__");
        if (!mainModule_) {
            throw std::runtime_error("Failed to import __main__");
        }
        mainDict_ = PyModule_GetDict(mainModule_);
    }

    ~PythonWrapper() {
        Py_XDECREF(mainModule_);
        Py_Finalize();
    }

    PyObject* matrixToPyList(const Matrix& matrix) {
        PyObject* outerList = PyList_New(matrix.rows());
        for (size_t i = 0; i < matrix.rows(); ++i) {
            PyObject* innerList = PyList_New(matrix.cols());
            for (size_t j = 0; j < matrix.cols(); ++j) {
                PyList_SET_ITEM(innerList, j, PyFloat_FromDouble(matrix(i, j)));
            }
            PyList_SET_ITEM(outerList, i, innerList);
        }
        return outerList;
    }

    Matrix pyListToMatrix(PyObject* list) {
        if (!PyList_Check(list)) {
            throw std::runtime_error("Expected a Python list");
        }

        size_t rows = PyList_Size(list);
        if (rows == 0) {
            return Matrix();
        }

        PyObject* firstRow = PyList_GetItem(list, 0);
        if (!PyList_Check(firstRow)) {
            throw std::runtime_error("Expected a list of lists");
        }

        size_t cols = PyList_Size(firstRow);
        Matrix result(rows, cols);

        for (size_t i = 0; i < rows; ++i) {
            PyObject* row = PyList_GetItem(list, i);
            if (!PyList_Check(row) || PyList_Size(row) != cols) {
                throw std::runtime_error("Inconsistent row sizes");
            }

            for (size_t j = 0; j < cols; ++j) {
                PyObject* item = PyList_GetItem(row, j);
                result(i, j) = PyFloat_AsDouble(item);
            }
        }

        return result;
    }

    IntMatrix pyListToIntMatrix(PyObject* list) {
        if (!PyList_Check(list)) {
            throw std::runtime_error("Expected a Python list");
        }

        size_t rows = PyList_Size(list);
        if (rows == 0) {
            return IntMatrix();
        }

        PyObject* firstRow = PyList_GetItem(list, 0);
        if (!PyList_Check(firstRow)) {
            throw std::runtime_error("Expected a list of lists");
        }

        size_t cols = PyList_Size(firstRow);
        IntMatrix result(rows, cols);

        for (size_t i = 0; i < rows; ++i) {
            PyObject* row = PyList_GetItem(list, i);
            if (!PyList_Check(row) || PyList_Size(row) != cols) {
                throw std::runtime_error("Inconsistent row sizes");
            }

            for (size_t j = 0; j < cols; ++j) {
                PyObject* item = PyList_GetItem(row, j);
                result(i, j) = PyLong_AsLong(item);
            }
        }

        return result;
    }

    PyObject* getMainDict() { return mainDict_; }

private:
    PyObject* mainModule_;
    PyObject* mainDict_;
};

class TetrahedralMesh {
public:
    TetrahedralMesh() : pythonWrapper_() {}

    bool generateMesh(
        const Matrix& cellCorners,
        const std::vector<bool>& isInside,
        const Matrix& intersectionPoints,
        const std::map<std::pair<int, int>, int>& edgeIntersections
    ) {
        try {
            // Get main dictionary
            PyObject* mainDict = pythonWrapper_.getMainDict();
            
            // Convert input data to Python objects
            PyObject* pyCellCorners = pythonWrapper_.matrixToPyList(cellCorners);
            PyObject* pyIntersectionPoints = pythonWrapper_.matrixToPyList(intersectionPoints);
            
            // Create Python list for isInside
            PyObject* pyIsInside = PyList_New(isInside.size());
            for (size_t i = 0; i < isInside.size(); ++i) {
                PyList_SetItem(pyIsInside, i, PyBool_FromLong(isInside[i]));
            }
            
            // Create Python dict for edgeIntersections
            PyObject* pyEdgeIntersections = PyDict_New();
            for (const auto& edge : edgeIntersections) {
                PyObject* key = PyTuple_New(2);
                PyTuple_SetItem(key, 0, PyLong_FromLong(edge.first.first));
                PyTuple_SetItem(key, 1, PyLong_FromLong(edge.first.second));
                PyDict_SetItem(pyEdgeIntersections, key, PyLong_FromLong(edge.second));
                Py_DECREF(key);
            }

            // Execute Python script
            FILE* fp = fopen("t3.py", "r");
            if (!fp) throw std::runtime_error("Failed to open t3.py");
            PyRun_SimpleFile(fp, "t3.py");
            fclose(fp);

            // Get Python function
            PyObject* tetrahedralizeFunc = PyDict_GetItemString(mainDict, "tetrahedralize_and_filter");
            if (!tetrahedralizeFunc) throw std::runtime_error("Failed to get tetrahedralize_and_filter function");

            // Call function
            PyObject* args = PyTuple_New(4);
            PyTuple_SetItem(args, 0, pyCellCorners);
            PyTuple_SetItem(args, 1, pyIsInside);
            PyTuple_SetItem(args, 2, pyIntersectionPoints);
            PyTuple_SetItem(args, 3, pyEdgeIntersections);

            PyObject* result = PyObject_CallObject(tetrahedralizeFunc, args);
            if (!result) throw std::runtime_error("Failed to call tetrahedralize_and_filter");

            // Extract results
            vertices_ = pythonWrapper_.pyListToMatrix(PyTuple_GetItem(result, 0));
            tetrahedra_ = pythonWrapper_.pyListToIntMatrix(PyTuple_GetItem(result, 1));

            // Cleanup
            Py_DECREF(args);
            Py_DECREF(result);
            
            return true;
        }
        catch (const std::exception& e) {
            if (PyErr_Occurred()) {
                PyErr_Print();
            }
            std::cerr << "Error: " << e.what() << std::endl;
            return false;
        }
    }

    void saveTecplot(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + filename);
        }

        file << "TITLE = \"Filtered Tetrahedral Mesh\"\n";
        file << "VARIABLES = \"X\", \"Y\", \"Z\"\n";
        file << "ZONE T=\"Filtered Mesh\", "
             << "N=" << vertices_.rows() << ", "
             << "E=" << tetrahedra_.rows() << ", "
             << "DATAPACKING=POINT, ZONETYPE=FETETRAHEDRON\n";

        file.precision(15);
        for (size_t i = 0; i < vertices_.rows(); ++i) {
            file << vertices_(i, 0) << " "
                 << vertices_(i, 1) << " "
                 << vertices_(i, 2) << "\n";
        }

        for (size_t i = 0; i < tetrahedra_.rows(); ++i) {
            file << (tetrahedra_(i, 0) + 1) << " "
                 << (tetrahedra_(i, 1) + 1) << " "
                 << (tetrahedra_(i, 2) + 1) << " "
                 << (tetrahedra_(i, 3) + 1) << "\n";
        }

        file.close();
    }

private:
    PythonWrapper pythonWrapper_;
    Matrix vertices_;
    IntMatrix tetrahedra_;
};

int main() {
    try {
        // Create input data
        Matrix cellCorners(8, 3);
        // Corner 0
        cellCorners(0,0) = 0.0; cellCorners(0,1) = 0.0; cellCorners(0,2) = 0.0;
        // Corner 1
        cellCorners(1,0) = 1.0; cellCorners(1,1) = 0.0; cellCorners(1,2) = 0.0;
        // Corner 2
        cellCorners(2,0) = 1.0; cellCorners(2,1) = 1.0; cellCorners(2,2) = 0.0;
        // Corner 3
        cellCorners(3,0) = 0.0; cellCorners(3,1) = 1.0; cellCorners(3,2) = 0.0;
        // Corner 4
        cellCorners(4,0) = 0.0; cellCorners(4,1) = 0.0; cellCorners(4,2) = 1.0;
        // Corner 5
        cellCorners(5,0) = 1.0; cellCorners(5,1) = 0.0; cellCorners(5,2) = 1.0;
        // Corner 6
        cellCorners(6,0) = 1.0; cellCorners(6,1) = 1.0; cellCorners(6,2) = 1.0;
        // Corner 7
        cellCorners(7,0) = 0.0; cellCorners(7,1) = 1.0; cellCorners(7,2) = 1.0;

        std::vector<bool> isInside = {true, false, false, false, true, false, false, false};

        Matrix intersectionPoints(4, 3);
        // Intersection points
        intersectionPoints(0,0) = 0.5; intersectionPoints(0,1) = 0.0; intersectionPoints(0,2) = 0.0;
        intersectionPoints(1,0) = 0.0; intersectionPoints(1,1) = 0.5; intersectionPoints(1,2) = 0.0;
        intersectionPoints(2,0) = 0.5; intersectionPoints(2,1) = 0.0; intersectionPoints(2,2) = 1.0;
        intersectionPoints(3,0) = 0.0; intersectionPoints(3,1) = 0.5; intersectionPoints(3,2) = 1.0;

        std::map<std::pair<int, int>, int> edgeIntersections;
        edgeIntersections[{0, 1}] = 0;
        edgeIntersections[{0, 3}] = 1;
        edgeIntersections[{4, 5}] = 2;
        edgeIntersections[{4, 7}] = 3;

        TetrahedralMesh mesh;
        if (mesh.generateMesh(cellCorners, isInside, intersectionPoints, edgeIntersections)) {
            mesh.saveTecplot("custom_mesh.dat");
            std::cout << "Mesh generated and saved successfully\n";
        } else {
            std::cerr << "Failed to generate mesh\n";
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
