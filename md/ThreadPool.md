<a name="biOjs"></a>
### 1. 线程池流程
1. 流程图解释：
   1. 这个是通用线程池，双端队列存放的是多个可调用对象（即用户任务function<void()>)，因此可以通过std::bind配接器传参。双端队列queue_，有时也称为工作队列。
   2. 其工作原理：
      1. 首先创建并启动一组线程，称为线程池threads_，由用户指定其大小maxQueueSize_，每个元素对对应一个线程。
      2. 每个线程函数都是一样的，在其中会运行一个loop循环：从双端队列取出一个任务对象task，如果非空，就执行之，如此往复。
   3. 当有一个用户线程想要通过线程池运行一个用户任务时，就可以将用户任务函数及参数封装成一个可调用对象Task f，然后通过线程池接口，将f加入双端队列末尾。当线程池有线程空闲时（未执行用户任务），就会从双端队列头部取出一个Task对象task，然后执行之

![](https://cdn.nlark.com/yuque/0/2022/png/26049599/1670313112584-a75fdd4d-4cc5-4655-a4f6-2f135ddae578.png#averageHue=%23faf9f9&clientId=u844fd9a0-5432-4&from=paste&id=u66fdbf75&originHeight=518&originWidth=876&originalType=url&ratio=1&rotation=0&showTitle=false&status=done&style=stroke&taskId=u74c7acb5-2e0d-4df5-8efb-83dcdb58757&title=)

