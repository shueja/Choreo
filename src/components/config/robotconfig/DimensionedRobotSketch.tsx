import { observer } from "mobx-react";
import { Expr, RobotConfig } from "../../../document/2025/DocumentTypes";
import { IRobotConfigStore } from "../../../document/RobotConfigStore";
import ExpressionInputList from "../../input/ExpressionInputList";
import ExpressionInput, { InputOnly } from "../../input/ExpressionInput";

type Props = {
    config: IRobotConfigStore,
    widthPx: number,
}
const STROKE_WIDTH = 0.01;
const DimensionedRobotSketch = (props: Props) =>{
    const {
        bumper,
        backLeft,
        frontLeft,
    } = props.config;
    const {
        front,
        back,
        side
    } = bumper;
    const viewSize = Math.max(
        back.value, side.value, front.value,
        frontLeft.x.value, frontLeft.y.value,
        -backLeft.x.value, backLeft.y.value
    ) + 0.1;
    const mToPx = props.widthPx / (2*viewSize);
    const inputWidth = "10ch";
    const minusInputWidth = "-10ch";
    const inputHeight = 24;
    return <svg viewBox={`${0} ${0} ${2*viewSize} ${2*viewSize}`} style={{
        width: "100%",
        height: "100%",
        }}>
        <g
        transform={`
        translate(${viewSize} ${viewSize})
              `}>
                <circle cx={0} cy={0} r={0.02} fill="white"></circle>
                
                {/* Left bumper edge */}
                <line x1={-back.value} x2={front.value} y1={-side.value} y2={-side.value} stroke="white" strokeWidth={STROKE_WIDTH}></line>
                {/* Left-front bumper edge */}
                <line x1={front.value} x2={front.value} y1={-side.value} y2={0} stroke="white" strokeWidth={STROKE_WIDTH}></line>
                {/* Left-back bumper edge */}
                <line x1={-back.value} x2={-back.value} y1={-side.value} y2={0} stroke="white" strokeWidth={STROKE_WIDTH}></line>
                <line x1={0} x2={0} y1={-side.value} y2={0} stroke="gray" strokeWidth={STROKE_WIDTH / 2}></line>
                {/* Right bumper edge */}
                <line x1={-back.value} x2={front.value} y1={side.value} y2={side.value} stroke="gray" strokeWidth={STROKE_WIDTH} strokeDasharray={3}></line>
                <g transform={`scale(${1/mToPx})`}>
                <foreignObject x={0}
                    y={-(side.value) * mToPx - inputHeight}
                    width={`max(10ch, ${front.value * mToPx}px)`}
                    height={inputHeight}>
                        <InputOnly title={""} enabled={true} number={front} inputStyles={{
                            textAlign: "center",
                            width: side.value * mToPx
                        }}></InputOnly>
                </foreignObject>

                <foreignObject
                    
                    x={0}
                    y={-(side.value) * mToPx - inputHeight}
                    width={`max(10ch, ${back.value * mToPx}px)`}
                    height={24}
                    style={{
                    transform:`translate(min(-10ch, -${back.value * mToPx}px), 0%)`}}>
                    <InputOnly title={""} enabled={true} number={back} inputStyles={{
                            textAlign: "center",
                            width: side.value * mToPx
                        }}></InputOnly>
                </foreignObject>
                </g>
        </g>
        
    </svg>
}

export default observer(DimensionedRobotSketch);