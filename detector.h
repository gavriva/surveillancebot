#pragma once

#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"
#include <algorithm>

class Detector {

    const int blurSize = 5;

    cv::Mat _dilateKernel;
    
    cv::Mat _gray;
    cv::Mat _gauss;
    cv::Mat _previous_frame;
    cv::Mat _delta;
    cv::Mat _delta_cooked;

    bool _first_frame;

public:

    std::vector<std::vector<cv::Point>> contours0;
    std::vector<cv::Vec4i>              hierarchy;

public:
    Detector()
        : _first_frame(true) {

        _dilateKernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(7, 7), cv::Point(-1, -1));
    }

    bool process_frame(cv::Mat frame) {

        cv::Mat pyr1 = frame.clone();

        cv::cvtColor(pyr1, _gray, cv::COLOR_BGR2GRAY);
        cv::GaussianBlur(_gray, _gauss, cv::Size(blurSize, blurSize), 5, 5);

        if (_first_frame) {
            _gauss.copyTo(_previous_frame);
            _first_frame = false;
        }
        
        cv::absdiff(_previous_frame, _gauss, _delta);
        _gauss.copyTo(_previous_frame);
        
        cv::Mat binary;

        cv::threshold(_delta, _delta, 15, 255, cv::THRESH_BINARY);

        // if (do_mask) {
        //     cv::bitwise_and(bin_mask, binary, binary);
        // }
        
        cv::dilate(_delta, _delta_cooked, _dilateKernel, cv::Point(-1, -1), 2);

        cv::findContours(_delta_cooked, contours0, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

        const auto num_pixels = double(frame.size().area());

        bool trigger = false;
        double totalArea = 0;
        double maxArea = 0;
        for (size_t k = 0; k < contours0.size(); ++k) {

            const auto area = contourArea(contours0[k]);

            totalArea += area;
            maxArea = std::max(maxArea, area);
        }

        totalArea /= num_pixels;
        maxArea /= num_pixels;

        if (maxArea > 0.05) {
            trigger = true;
        }

        if (totalArea >= 0.1) {
            trigger = true;
        }

        spdlog::info("Max area {},  sum of areas {}", maxArea, totalArea);

        return trigger;
    }

    void debug(int v) {
        //Debug window
        switch (v)
        {
        case 1:
            cv::imshow("Debug", _gauss); //Noise reduction
            break;
        case 2:
            cv::imshow("Debug", _delta); //Difference image
            break;
        //case 3:
          //  cv::imshow("Debug", binary); //Binary threshold image
            //break;
        case 4:
            cv::imshow("Debug", _delta_cooked); //Dilated
            break;
        default: //Should never happen
            break;
        }
    }

};