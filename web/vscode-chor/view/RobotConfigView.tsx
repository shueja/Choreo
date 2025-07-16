import { useContext, useEffect, useState } from "react";
import { StateStoreContext } from "../state/State";
import { observer } from "mobx-react";
import { IExpressionStore, math } from "$src/document/ExpressionStore"
import { VscodeTextfield } from "@vscode-elements/elements";
import "@vscode-elements/elements/dist/vscode-table";
import "@vscode-elements/elements/dist/vscode-table-body";
import "@vscode-elements/elements/dist/vscode-table-row";
import "@vscode-elements/elements/dist/vscode-table-header";
import "@vscode-elements/elements/dist/vscode-table-header-cell";
import "@vscode-elements/elements/dist/vscode-table-cell";
import "@vscode-elements/elements/dist/vscode-textfield";
import "@vscode-elements/elements/dist/vscode-single-select";
import { autorun } from "mobx";
import { MathNode } from "mathjs";

type VSCodeExpressionInputProps = {
    expr: IExpressionStore
}
export const VSCodeExpressionInput = observer((props: VSCodeExpressionInputProps) => {
    
    const [editedValue, setEditedValue] = useState(props.expr.expr.toString());
    const [errors, setErrors] = useState(false);
    const validate =  (expression: string) : MathNode | undefined => {
        try {
            return props.expr.validate(
              math.parse(expression)
            )
        } catch (e) {
            return undefined;
        }
    }
    const handleChange = (event:any) => {
        
        let valid = validate(event.target.value) !== undefined;
        console.log(valid, event.target.value);
        setErrors(!valid);
        setEditedValue(event.target.value);
    };
    const handleSubmit = (event:any) => {
        console.log(event);
        const newNode = validate(event.target.value);
        if (newNode !== undefined && !newNode.equals(props.expr.expr)) {
            props.expr.set(newNode);
            console.log(newNode.toString());
        } else {
            setEditedValue(props.expr.expr.toString());
            event.target.blur();
            setErrors(!props.expr.valid);
        }
    }
    useEffect(()=>autorun(()=>setEditedValue(props.expr.expr.toString())), []);

    return (<vscode-textfield class="popover" onchange={handleSubmit} onInput={handleChange}
        value={editedValue}
        invalid={errors}
    >

    </vscode-textfield>)
})

type RobotConfigRowProps = {
    title: string,
    expr: IExpressionStore,
}
const RobotConfigRow = observer((props: RobotConfigRowProps) => (
    <vscode-table-row>
        <vscode-table-cell>{props.title}</vscode-table-cell>
        <vscode-table-cell>
            <VSCodeExpressionInput expr={props.expr}></VSCodeExpressionInput>
        </vscode-table-cell>
    </vscode-table-row>
))
const RobotConfigView = observer(() => {
    const stateStore = useContext(StateStoreContext)!;
    const robotConfig = stateStore.config;
    return (
        <>
            <vscode-table zebra bordered-rows breakpoint={400} responsive style={{maxHeight:"100%", height:"100%"}}>
                <vscode-table-body slot="body">
                    <RobotConfigRow title="Mass" expr={robotConfig.mass}></RobotConfigRow>
                    <RobotConfigRow title="MoI" expr={robotConfig.inertia}></RobotConfigRow>
                    <RobotConfigRow title="Bumper Front" expr={robotConfig.bumper.front}></RobotConfigRow>
                    <RobotConfigRow title="Bumper Back" expr={robotConfig.bumper.back}></RobotConfigRow>
                    <RobotConfigRow title="Bumper Side" expr={robotConfig.bumper.side}></RobotConfigRow>
                    <RobotConfigRow title="Wheel Radius" expr={robotConfig.radius}></RobotConfigRow>
                    <RobotConfigRow title="Wheel COF" expr={robotConfig.cof}></RobotConfigRow>
                    <RobotConfigRow title="Motor Rev/Wheel Rev" expr={robotConfig.gearing}></RobotConfigRow>
                    <RobotConfigRow title="Motor Max Speed" expr = {robotConfig.vmax}></RobotConfigRow>
                    <RobotConfigRow title="Motor Max Torque" expr={robotConfig.tmax}></RobotConfigRow>
                    <vscode-table-row>
                    <vscode-table-cell>Drive Type</vscode-table-cell>
                    <vscode-table-cell>
                    <vscode-single-select>
                        <vscode-option description="Consectetur adipiscing elit">Swerve</vscode-option>
                        <vscode-option description="Donec elit odio">Differential</vscode-option>
                    </vscode-single-select>
                    </vscode-table-cell>
                    </vscode-table-row>
                    
                    {/* <RobotConfigRow title="Max Motor Torque" expr={robotConfig.tmax}></RobotConfigRow>
                    <RobotConfigRow title="Max Motor Velocity" expr = {robotConfig.vmax}></RobotConfigRow> */}
                    
                </vscode-table-body>
            </vscode-table>
        </>
    )
})

export default RobotConfigView;