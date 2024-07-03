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

//Тут пулл потоков
class ThreadPool 
{

private:
    std::vector<std::thread> workers; //Это у нас, как описывалось вектор полный потоков по имени Воркерс
    std::queue<std::function<void()>> tasks;//Это очередь по имени Таскс, которая хранит в себе функции
    std::mutex queue_mutex;  //мьютекс, о котором говорили всё многопоточие
    std::condition_variable condition; // и ключевое слово для замыкания
    bool stop; //индикатор, остановки, который мы почему-то зовём флагом


public:
    ThreadPool(size_t threads) : stop(false) // инициируем переменную сразу, в конструкторе
    {
        for (size_t i = 0; i < threads; ++i)//делаем цикл для всех потоков, от первого, до последнего
        {
            workers.emplace_back([this]// используется для вставки нового элемента в конец вектора
                {
                for (;;) //вместо вайл
                {
                    std::function<void()> task; //делаем обьект таск, который сохранит в себя функцию 
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);//использыем мьютекс и 
                        this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });//соответстевнно волшебное слова класса кондишин вариабле
                        if (this->stop && this->tasks.empty()) //если переменная годна, и обьект пуст
                        {
                            return; //конец кода
                        }
                        task = std::move(this->tasks.front()); //в обьект таск мы пихаем ту первую функцию которую хранит очередь
                        this->tasks.pop();//из очережи соответстенно удаляем
                    }
                    task();// обьект подготовлен к работе
                }
                });
        }
    }

    template<class F, class... Args> //делаем шаблон сдвумя параметрами шаблона: "F" и "Args". Шаблон "F" представляет собой тип функции, а пакет "Args" представляет собой набор типов аргументов функции.
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type> 
    {//Это объявление шаблонной функции enqueue, которая принимает универсальные ссылки на функцию f и произвольное количество аргументов args.Возвращаемый тип функции — это std::future, который будет содержать результат выполнения функции f.
       
        using return_type = typename std::result_of<F(Args...)>::type;//Здесь определяется тип return_type, который является типом результата функции f, когда она вызывается с аргументами args.


        auto task = std::make_shared<std::packaged_task<return_type()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        //указатель task на std::packaged_task, который инкапсулирует функцию f и её аргументы args. std::bind используется для связывания функции с её аргументами, а std::forward используется для пересылки аргументов с сохранением их типа (lvalue или rvalue).

        std::future<return_type> res = task->get_future();
        {//Создается объект std::future, который будет использоваться для получения результата выполнения задачи.

            std::unique_lock<std::mutex> lock(queue_mutex);//Создается блокировка lock с помощью std::unique_lock, которая захватывает мьютекс queue_mutex, обеспечивая тем самым безопасный доступ к общим ресурсам.

            if (stop) 
            { //Если пул потоков был остановлен (переменная stop установлена в true), то генерируется исключение std::runtime_error.
                throw std::runtime_error("Пулл поставить на очередь");
            }
            tasks.emplace([task]() { (*task)(); }); //В очередь tasks добавляется новая задача.Лямбда - функция захватывает task и вызывает его при выполнении.
        }
        condition.notify_one();
        //После добавления задачи в очередь, один из ожидающих потоков уведомляется с помощью переменной условия condition.
        return res;
        //Функция возвращает объект std::future, который может быть использован для ожидания завершения задачи и получения результата её выполнения.
    }

    ~ThreadPool()//деструктор
    {
        {
            std::unique_lock<std::mutex> lock(queue_mutex); 
            //Создается объект std::unique_lock, который блокирует мьютекс queue_mutex.Это делается для обеспечения безопасного доступа к флагу stop, который используется для контроля работы потоков.
            stop = true;
            //Помечается флаг stop, указывающий на то, что работа потоков должна быть остановлена. Этот флаг будет использоваться в цикле потоков для определения, нужно ли им завершить свою работу.
        }
        condition.notify_all();
        //notify_all(); - Вызывается метод notify_all объекта condition, оповещающий все потоки, ожидающие на данном условном переменной, что они могут проверить условие и, возможно, продолжить выполнение.
        for (std::thread& worker : workers) 
        {//Здесь происходит итерация по всем потокам из вектора workers и вызывается метод join(), который блокирует текущий поток до тех пор, пока соответствующий поток из вектора не завершит свою работу. Это позволяет убедиться, что все потоки завершили свою работу перед разрушением пула потоков.
            worker.join();
        }
    }


};

// Функция для сортировки части массива (о боже мой, это просто кошмар)
void quicksort(std::vector<int>& array, int left, int right, ThreadPool& pool, std::shared_ptr<std::atomic<int>> counter, std::shared_ptr<std::promise<void>> done)
{ //ThreadPool& pool: это пул потоков, который будет использоваться для выполнения параллельных операций сортировки;
    //std::shared_ptr<std::atomic<int>> counter: это счетчик, который будет использоваться для подсчета количества операций сортировки;
    //std::shared_ptr<std::promise<void>> done: это объект promise, который будет использоваться для уведомления об окончании сортировки.
    if (left < right) {
        int pivot = array[(left + right) / 2];
        int i = left, j = right;
        while (i <= j) {
            while (array[i] < pivot) i++;
            while (array[j] > pivot) j--;
            if (i <= j) {
                std::swap(array[i], array[j]);// свап, который был в самом начале курса
                i++;
                j--;
            }
        }

        // количество подзадач
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

        // Уменьшаем счетчик
        if (--(*counter) == 0) {
            done->set_value();
        }
    }
    else {
        // Если задач больше нет делаем
        if (--(*counter) == 0) {
            done->set_value();
        }
    }
}

int main() {

    setlocale(LC_ALL, "ru");


    std::vector<int> array = { };//инициируем массив
    ThreadPool pool(std::thread::hardware_concurrency());
    auto counter = std::make_shared<std::atomic<int>>(1);
    auto done = std::make_shared<std::promise<void>>();

    auto future = done->get_future();
    pool.enqueue([&array, &pool, counter, done] {
        quicksort(array, 0, array.size() - 1, pool, counter, done);});

    future.wait();
    std::cout << "Наконец-то после тысячи попыток сортировка завершена!" << std::endl;

    return 0;
}