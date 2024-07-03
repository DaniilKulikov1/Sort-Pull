#include <iostream>
#include <vector>
#include <future>
#include <algorithm>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <memory>
#include <atomic>

//��� ���� �������
class ThreadPool 
{

private:
    std::vector<std::thread> workers; //��� � ���, ��� ����������� ������ ������ ������� �� ����� �������
    std::queue<std::function<void()>> tasks;//��� ������� �� ����� �����, ������� ������ � ���� �������
    std::mutex queue_mutex;  //�������, � ������� �������� �� ������������
    std::condition_variable condition; // � �������� ����� ��� ���������
    bool stop; //���������, ���������, ������� �� ������-�� ���� ������


public:
    ThreadPool(size_t threads) : stop(false) // ���������� ���������� �����, � ������������
    {
        for (size_t i = 0; i < threads; ++i)//������ ���� ��� ���� �������, �� �������, �� ����������
        {
            workers.emplace_back([this]// ������������ ��� ������� ������ �������� � ����� �������
                {
                for (;;) //������ ����
                {
                    std::function<void()> task; //������ ������ ����, ������� �������� � ���� ������� 
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);//���������� ������� � 
                        this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });//�������������� ��������� ����� ������ �������� ��������
                        if (this->stop && this->tasks.empty()) //���� ���������� �����, � ������ ����
                        {
                            return; //����� ����
                        }
                        task = std::move(this->tasks.front()); //� ������ ���� �� ������ �� ������ ������� ������� ������ �������
                        this->tasks.pop();//�� ������� ������������� �������
                    }
                    task();// ������ ����������� � ������
                }
                });
        }
    }

    template<class F, class... Args> //������ ������ ������ ����������� �������: "F" � "Args". ������ "F" ������������ ����� ��� �������, � ����� "Args" ������������ ����� ����� ����� ���������� �������.
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type> 
    {//��� ���������� ��������� ������� enqueue, ������� ��������� ������������� ������ �� ������� f � ������������ ���������� ���������� args.������������ ��� ������� � ��� std::future, ������� ����� ��������� ��������� ���������� ������� f.
       
        using return_type = typename std::result_of<F(Args...)>::type;//����� ������������ ��� return_type, ������� �������� ����� ���������� ������� f, ����� ��� ���������� � ����������� args.


        auto task = std::make_shared<std::packaged_task<return_type()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        //��������� task �� std::packaged_task, ������� ������������� ������� f � � ��������� args. std::bind ������������ ��� ���������� ������� � � �����������, � std::forward ������������ ��� ��������� ���������� � ����������� �� ���� (lvalue ��� rvalue).

        std::future<return_type> res = task->get_future();
        {//��������� ������ std::future, ������� ����� �������������� ��� ��������� ���������� ���������� ������.

            std::unique_lock<std::mutex> lock(queue_mutex);//��������� ���������� lock � ������� std::unique_lock, ������� ����������� ������� queue_mutex, ����������� ��� ����� ���������� ������ � ����� ��������.

            if (stop) 
            { //���� ��� ������� ��� ���������� (���������� stop ����������� � true), �� ������������ ���������� std::runtime_error.
                throw std::runtime_error("���� ��������� �� �������");
            }
            tasks.emplace([task]() { (*task)(); }); //� ������� tasks ����������� ����� ������.������ - ������� ����������� task � �������� ��� ��� ����������.
        }
        condition.notify_one();
        //����� ���������� ������ � �������, ���� �� ��������� ������� ������������ � ������� ���������� ������� condition.
        return res;
        //������� ���������� ������ std::future, ������� ����� ���� ����������� ��� �������� ���������� ������ � ��������� ���������� � ����������.
    }

    ~ThreadPool()//����������
    {
        {
            std::unique_lock<std::mutex> lock(queue_mutex); 
            //��������� ������ std::unique_lock, ������� ��������� ������� queue_mutex.��� �������� ��� ����������� ����������� ������� � ����� stop, ������� ������������ ��� �������� ������ �������.
            stop = true;
            //���������� ���� stop, ����������� �� ��, ��� ������ ������� ������ ���� �����������. ���� ���� ����� �������������� � ����� ������� ��� �����������, ����� �� �� ��������� ���� ������.
        }
        condition.notify_all();
        //notify_all(); - ���������� ����� notify_all ������� condition, ����������� ��� ������, ��������� �� ������ �������� ����������, ��� ��� ����� ��������� ������� �, ��������, ���������� ����������.
        for (std::thread& worker : workers) 
        {//����� ���������� �������� �� ���� ������� �� ������� workers � ���������� ����� join(), ������� ��������� ������� ����� �� ��� ���, ���� ��������������� ����� �� ������� �� �������� ���� ������. ��� ��������� ���������, ��� ��� ������ ��������� ���� ������ ����� ����������� ���� �������.
            worker.join();
        }
    }


};

// ������� ��� ���������� ����� ������� (� ���� ���, ��� ������ ������)
void quicksort(std::vector<int>& array, int left, int right, ThreadPool& pool, std::shared_ptr<std::atomic<int>> counter, std::shared_ptr<std::promise<void>> done)
{ //ThreadPool& pool: ��� ��� �������, ������� ����� �������������� ��� ���������� ������������ �������� ����������;
    //std::shared_ptr<std::atomic<int>> counter: ��� �������, ������� ����� �������������� ��� �������� ���������� �������� ����������;
    //std::shared_ptr<std::promise<void>> done: ��� ������ promise, ������� ����� �������������� ��� ����������� �� ��������� ����������.
    if (left < right) {
        int pivot = array[(left + right) / 2];
        int i = left, j = right;
        while (i <= j) {
            while (array[i] < pivot) i++;
            while (array[j] > pivot) j--;
            if (i <= j) {
                std::swap(array[i], array[j]);// ����, ������� ��� � ����� ������ �����
                i++;
                j--;
            }
        }

        // ���������� ��������
        (*counter)++;
        auto left_done = std::make_shared<std::promise<void>>();
        auto left_counter = counter;

        if ((right - left) > 100000) {
            pool.enqueue([=, &array, &pool] {
                quicksort(array, left, j, pool, left_counter, left_done);
                left_done->set_value();
                });
        }
        else {
            quicksort(array, left, j, pool, left_counter, left_done);
            left_done->set_value();
        }

        quicksort(array, i, right, pool, counter, done);

        left_done->get_future().wait();

        // ��������� �������
        if (--(*counter) == 0) {
            done->set_value();
        }
    }
    else {
        // ���� ����� ������ ��� ������
        if (--(*counter) == 0) {
            done->set_value();
        }
    }
}

int main() {

    setlocale(LC_ALL, "ru");


    std::vector<int> array = { };//���������� ������
    ThreadPool pool(std::thread::hardware_concurrency());
    auto counter = std::make_shared<std::atomic<int>>(1);
    auto done = std::make_shared<std::promise<void>>();

    auto future = done->get_future();
    pool.enqueue([&array, &pool, counter, done] {
        quicksort(array, 0, array.size() - 1, pool, counter, done);});

    future.wait();
    std::cout << "�������-�� ����� ������ ������� ���������� ���������!" << std::endl;

    return 0;
}