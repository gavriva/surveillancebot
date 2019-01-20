#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <chrono>
#include <signal.h>

#define FMT_HEADER_ONLY
#include "libs/fmt/include/fmt/format.h"
#include "libs/fmt/include/fmt/chrono.h"
#include "libs/date/include/date/date.h"

#include "libs/spdlog/include/spdlog/spdlog.h"
#include "libs/spdlog/include/spdlog/sinks/stdout_color_sinks.h"

#include "detector.h"

static void execCommand(const std::string &command)
{
    ::system(command.c_str());
}

const cv::String argKeys =
    "{help h ?     |          | print this message}"
    "{o            | event1v  | output file template}"
    "{i input      | <none>   | input video source}"
    "{m mask       | <none>   | mask file}"
    "{d            | 0        | debug mode}"
    "{g            |          | Generate template image for ROI mask.}"
    "{name         |          | Name of camera.}"
    "{ts timestamp |          | put time stamp on video }";

void main2(int argc, char **argv)
{
    cv::CommandLineParser parser(argc, argv, argKeys);
    parser.about("Application name v1.0.0");

    if (parser.has("help"))
    {
        parser.printMessage();
        spdlog::info("help");
        return;
    }

    if (!parser.check())
    {
        parser.printErrors();
        spdlog::info("errors in arguments");
        return;
    }

    /*
    Settings settings;

    if (settings.load_from_args(argc, argv) != 0)
    {
        exit(-1);
    }*/

    ::signal(SIGPIPE, SIG_IGN);

    int counter = 0; // event counter

    cv::VideoCapture cap;

    //If no provided input source, try to open the
    //first device's camera
    if (parser.has("i"))
    {
        const auto input = parser.get<std::string>("input");
        if (!cap.open(input))
        {
            spdlog::error("Cannot open '{}'", input);
            return;
        }
        spdlog::info("Opened {}", input);
    }
    else
    {
        //cap.set(cv::CAP_PROP_FPS, settings.fps);
        cap.open(0);
    }

    const auto size = cv::Size((int)cap.get(cv::CAP_PROP_FRAME_WIDTH), (int)cap.get(cv::CAP_PROP_FRAME_HEIGHT));

    if (parser.has("g"))
    {
        cv::Mat frame;
        cap >> frame;

        std::vector<int> compression_params;
        compression_params.push_back(cv::IMWRITE_PNG_COMPRESSION);
        compression_params.push_back(9);

        try
        {
            cv::imwrite("mask.png", frame, compression_params);
            spdlog::info("Saved template for ROI Mask image.");
        }
        catch (std::exception &ex)
        {
            spdlog::error("Exception converting image to PNG format: {}", ex.what());
        }
        return;
    }

    // Load the ROI mask image.

    // bool do_mask = false;
    // if (settings.mask_file != "")
    // {
    //     do_mask = true;
    //     mask = cv::imread(settings.mask_file, cv::IMREAD_GRAYSCALE);
    //     cv::threshold(mask, bin_mask, 15, 255, cv::THRESH_BINARY);
    // }

    cv::VideoWriter writer;
    int motion = 0;
    int frame_no = 0;

    Detector detector;

    const auto cam_name = parser.get<std::string>("name");

    cv::Mat frame;

    while (true)
    {
        cap >> frame;
        //spdlog::info("get frame {}  {} x {}", frame_no++, frame.size().width, frame.size().height);

        if (frame.empty())
            break;

        if (detector.process_frame(frame))
        {
            motion = 1;
        }
        else
        {
            --motion;
        }

        if (!cam_name.empty())
        {
            putText(frame, cam_name, cv::Point2f(15, 25), cv::FONT_HERSHEY_PLAIN, 2, cv::Scalar(0, 0, 255, 255), 2);
        }

        if (parser.has("ts"))
        {
            const auto tm = date::format("%Y-%m-%d %H:%M:%S", date::floor<std::chrono::seconds>(std::chrono::system_clock::now()));
            putText(frame, tm, cv::Point2f(15, size.height - 15), cv::FONT_HERSHEY_PLAIN, 1, cv::Scalar(0, 0, 255, 255), 2);
        }

        if (parser.get<int>("d") > 0 && writer.isOpened())
        {
            // Draw contours
            cv::Mat drawing = cv::Mat::zeros(frame.size(), CV_8UC3);
            cv::RNG rng(12345);
            for (int i = 0; i < detector.contours0.size(); i++)
            {
                cv::Scalar color = cv::Scalar(rng.uniform(0, 255), rng.uniform(0, 255), rng.uniform(0, 255));
                cv::drawContours(drawing, detector.contours0, i, color, 1, 8, detector.hierarchy, 0, cv::Point());
            }
            add(frame, drawing, frame);
        }

        if (writer.isOpened())
        {
            if (motion < -50)
            {
                writer.release();
                spdlog::info("MotionStop");
            }
        }
        else if (motion > 0)
        {
            spdlog::info("Motion Start");
            counter++;

            // TODO: implement support of motion cs https://motion-project.github.io/motion_config.html#conversion_specifiers

            const auto filename = fmt::format("event{:03d}.mkv", counter);
            if (writer.open(filename, cv::VideoWriter::fourcc('X', 'V', 'I', 'D'), cap.get(cv::CAP_PROP_FPS), size))  // see http://www.fourcc.org/codecs.php
            {
                spdlog::info("Recording file: {}  {} fps  {} x {}", filename, cap.get(cv::CAP_PROP_FPS), size.width, size.height);
            }
            else
            {
                spdlog::error("Cannot write to file: {}  {} fps  {} x {}", filename, cap.get(cv::CAP_PROP_FPS), size.width, size.height);
            }
        }

        if (parser.get<int>("d") > 0)
        {
            cv::namedWindow("Input", cv::WINDOW_AUTOSIZE);
            cv::imshow("Input", frame);
            detector.debug(parser.get<int>("d"));
            if (cv::waitKey(133) == 0)
                return;
        }

        if (writer.isOpened())
            writer.write(frame);
    }
    return;
}

int main(int argc, char **argv)
{

    try
    {
        main2(argc, argv);
    }
    catch (std::exception &ex)
    {
        spdlog::error("Exception: {}\n", ex.what());
        return 1;
    }
    return 0;
}