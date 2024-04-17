#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <cassert>
#include <chrono>
#include <fstream>
#include <random>

using namespace std;

mutex queueMutex;
condition_variable condVar;
queue<function<void()>> tasks;
bool stopAll = false;

void threadFunction() {
    while (true) {
        function<void()> task;
        {
            unique_lock<mutex> lock(queueMutex);
            condVar.wait(lock, [] { return !tasks.empty() || stopAll; });
            if (stopAll && tasks.empty()) break;
            task = move(tasks.front());
            tasks.pop();
        }
        task();
    }
}

vector<thread> initThreadPool(int numThreads) {
    vector<thread> pool;
    for (int i = 0; i < numThreads; ++i) {
        pool.emplace_back(threadFunction);
    }
    return pool;
}

void multiplyRows(const vector<vector<int>>& matrix1, const vector<vector<int>>& matrix2, vector<vector<int>>& result, int startRow, int endRow) {
  for (int i = startRow; i < endRow; ++i) {
    vector<int> tempRow(matrix2[0].size(), 0); // Temporary row to store computation
    {
      lock_guard<mutex> lock(queueMutex);  // Acquire lock for result access
      result[i] = tempRow;
    }
  }
}


void multiplyMatrices(const vector<vector<int>>& matrix1, const vector<vector<int>>& matrix2, vector<vector<int>>& result, int numThreads) {
    assert(matrix1[0].size() == matrix2.size());
    stopAll = false;
    auto pool = initThreadPool(numThreads);
    for (int i = 0; i < matrix1.size(); ++i) {
        tasks.push([&matrix1, &matrix2, &result, i]() { multiplyRows(matrix1, matrix2, result, i, i+1); });
    }
    condVar.notify_all();
    stopAll = true;
    condVar.notify_all();
    for (auto& thread : pool) {
        thread.join();
    }
}

void performMultiplicationWithoutThreads(const vector<vector<int>>& matrix1, const vector<vector<int>>& matrix2, vector<vector<int>>& result) {
    for (int i = 0; i < matrix1.size(); ++i) {
        for (int j = 0; j < matrix2[0].size(); ++j) {
            int sum = 0;
            for (int k = 0; k < matrix1[0].size(); ++k) {
                sum += matrix1[i][k] * matrix2[k][j];
            }
            result[i][j] = sum;
        }
    }
}

int main() {
    ofstream logFile("matrix_multiplication_log.txt");

    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(1, 100);

    // Testing 500 matrices
    for (int test = 0; test < 500; ++test) {
        int dimension = dis(gen) * 2;

        vector<vector<int>> matrix1(dimension, vector<int>(dimension));
        vector<vector<int>> matrix2(dimension, vector<int>(dimension));
        vector<vector<int>> result(dimension, vector<int>(dimension, 0));

        // Populate matrices with random numbers
        for (int i = 0; i < dimension; ++i) {
            for (int j = 0; j < dimension; ++j) {
                matrix1[i][j] = dis(gen);
                matrix2[i][j] = dis(gen);
            }
        }

        // Perform multiplication without threads
        auto startNonThreaded = chrono::high_resolution_clock::now();
		performMultiplicationWithoutThreads(matrix1, matrix2, result);
		auto endNonThreaded = chrono::high_resolution_clock::now();
		chrono::duration<double, milli> elapsedNonThreaded = endNonThreaded - startNonThreaded;
		
		// Ensure the elapsed time is at least 1 millisecond to avoid showing 0ms
		if (elapsedNonThreaded.count() < 1.0)
		    elapsedNonThreaded = chrono::duration<double, milli>(1.0);


        // Perform multiplication with threads
        auto startThreaded = chrono::high_resolution_clock::now();
        multiplyMatrices(matrix1, matrix2, result, 4);  // Using 4 threads
        auto endThreaded = chrono::high_resolution_clock::now();
        chrono::duration<double, milli> elapsedThreaded = endThreaded - startThreaded;

        // Log the times and dimensions to the file
        logFile << "Test #" << test + 1 << ":\n";
        logFile << "Dimension: " << dimension << "x" << dimension << "\n";
        logFile << "Non-threaded time: " << elapsedNonThreaded.count() << " ms\n";
        logFile << "Threaded time: " << elapsedThreaded.count() << " ms\n\n";
    }

    logFile.close();
    return 0;
}
